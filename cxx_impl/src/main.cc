// ISC License
//
// Copyright (c) 2025 Stephen Seo
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

// Standard library includes
#include <bitset>
#include <chrono>
#include <cstring>
#include <iostream>
#include <optional>
#include <thread>
#include <tuple>
#include <unordered_map>

// Unix includes
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

// Third party includes
#include <curl/curl.h>

// Local includes.
#include "args.h"
#include "constants.h"
#include "db.h"
#include "db_msql.h"
#include "helpers.h"
#include "http.h"
#include "poor_mans_print.h"

volatile int interrupt_received = 0;

void receive_signal(int sig) {
  PMA_Println("Interupt received...");
  // SIGHUP, SIGINT, SIGTERM
  // if (sig == 1 || sig == 2 || sig == 15) {
  //}
  interrupt_received = 1;
}

struct AddrPortInfo {
  std::optional<std::string> remaining_buffer;
  std::string host_addr;
  std::string client_addr;
  // 0 - is ipv4
  std::bitset<16> flags;
  uint16_t port;
  uint16_t ticks;
};

AddrPortInfo conv_addr_port(const PMA_ARGS::AddrPort &addr_port, bool is_ipv4) {
  AddrPortInfo info = {std::nullopt,      std::get<0>(addr_port), std::string{},
                       std::bitset<16>{}, std::get<1>(addr_port), 0};

  info.flags.set(0, is_ipv4);

  return info;
}

size_t pma_curl_data_callback(void *buf, size_t size, size_t nmemb, void *ud) {
  std::string *res = reinterpret_cast<std::string *>(ud);
  res->append(reinterpret_cast<char *>(buf), size * nmemb);
  return size * nmemb;
}

size_t pma_curl_header_callback(void *buf, size_t size, size_t nitems,
                                void *ud) {
  std::unordered_map<std::string, std::string> *header_map =
      reinterpret_cast<std::unordered_map<std::string, std::string> *>(ud);
  const char *char_buf = reinterpret_cast<char *>(buf);
  std::string key, val;
  bool get_key = true;
  bool get_val = false;
  for (size_t idx = 0; idx < size * nitems; ++idx) {
    if (get_key) {
      if (char_buf[idx] == ':') {
        get_key = false;
        get_val = true;
      } else {
        key.push_back(char_buf[idx]);
      }
    } else if (get_val) {
      if (char_buf[idx] == ' ' || char_buf[idx] == '\n' ||
          char_buf[idx] == '\r') {
        continue;
      } else {
        val.push_back(char_buf[idx]);
      }
    }
  }

  if (!key.empty() && !val.empty()) {
    header_map->emplace(PMA_HELPER::ascii_str_to_lower(key), val);
  }

  return size * nitems;
}

size_t pma_curl_body_send_callback(char *buf, size_t size, size_t nitems,
                                   void *ud) {
  void **ptrs = reinterpret_cast<void **>(ud);
  std::string *body = reinterpret_cast<std::string *>(ptrs[0]);
  size_t *count = reinterpret_cast<size_t *>(ptrs[1]);

  size_t min = (size * nitems) < body->size() ? size * nitems : body->size();
  std::memcpy(buf, body->data() + *count, min);
  *count += min;

  return min;
}

// Returns true if "goto PMA_RESPONSE_SEND_LOCATION" is required.
void do_curl_forwarding(std::string cli_addr, uint16_t cli_port,
                        std::string &body, std::string &status,
                        std::string &content_type, const PMA_HTTP::Request &req,
                        const PMA_ARGS::Args &args) {
  CURLcode pma_curl_ret;
  CURL *curl_handle = curl_easy_init();
  GenericCleanup<CURL *> pma_curl_cleanup(
      curl_handle, [](CURL **handle) { curl_easy_cleanup(*handle); });

#ifndef NDEBUG
  pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);
  if (pma_curl_ret != CURLE_OK) {
    PMA_EPrintln(
        "ERROR: Failed to set curl verbose (client {}, port "
        "{})!",
        cli_addr, cli_port);
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to set "
        "curl verbose</p></html>";
    return;
  }
#endif

  // Set curl destination
  if (auto header_iter = req.headers.find("override-dest-url");
      header_iter != req.headers.end() && args.flags.test(1)) {
    std::string req_url = header_iter->second;
    while (req_url.ends_with('/')) {
      req_url.pop_back();
    }
    req_url.append(req.full_url);
    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_URL, req_url.c_str());
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl destination (client {}, "
          "port "
          "{})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to "
          "set "
          "curl url</p></html>";
      return;
    }
  } else if (auto url_iter = args.port_to_dest_urls.find(cli_port);
             url_iter != args.port_to_dest_urls.end()) {
    std::string req_url = url_iter->second;
    while (req_url.ends_with('/')) {
      req_url.pop_back();
    }
    req_url.append(req.full_url);
    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_URL, req_url.c_str());
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl destination (client {}, "
          "port "
          "{})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to "
          "set "
          "curl url</p></html>";
      return;
    }
  } else {
    std::string req_url = args.default_dest_url;
    while (req_url.ends_with('/')) {
      req_url.pop_back();
    }
    req_url.append(req.full_url);
    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_URL, req_url.c_str());
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl destination (client {}, "
          "port "
          "{})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to "
          "set "
          "curl url</p></html>";
      return;
    }
  }

  // Set curl follow redirects
  pma_curl_ret =
      curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, CURLFOLLOW_ALL);
  if (pma_curl_ret != CURLE_OK) {
    PMA_EPrintln(
        "ERROR: Failed to set curl follow redirects (client {}, "
        "port {})!",
        cli_addr, cli_port);
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to set "
        "curl follow redirects</p></html>";
    return;
  }

  // Set curl http headers
  struct curl_slist *headers_list = nullptr;
  GenericCleanup<struct curl_slist **> headers_cleanup(
      &headers_list,
      [](struct curl_slist ***list) { curl_slist_free_all(**list); });
  headers_list = curl_slist_append(
      headers_list, "accept: text/html,application/xhtml+xml,*/*");
  if (auto ip_iter = req.headers.find("x-real-ip");
      ip_iter != req.headers.end() && args.flags.test(0)) {
    headers_list = curl_slist_append(
        headers_list, std::format("x-real-ip: {}", ip_iter->second).c_str());
  }
  if (auto type_iter = req.headers.find("content-type");
      type_iter != req.headers.end()) {
    headers_list = curl_slist_append(
        headers_list,
        std::format("content-type: {}", type_iter->second).c_str());
  }
  // for (const auto &pair : req.headers) {
  //   if (pair.first == "host" || pair.first ==
  //   "override-dest-url") {
  //     continue;
  //   }
  //   headers_list = curl_slist_append(
  //       headers_list,
  //       std::format("{}: {}", pair.first,
  //       pair.second).c_str());
  // }
  pma_curl_ret =
      curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers_list);
  if (pma_curl_ret != CURLE_OK) {
    PMA_EPrintln("ERROR: Failed to set curl headers (client {}, port {})!",
                 cli_addr, cli_port);
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to set "
        "curl headers</p></html>";
    return;
  }

  // Set callback for fetched data
  body.clear();
  pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION,
                                  pma_curl_data_callback);
  if (pma_curl_ret != CURLE_OK) {
    PMA_EPrintln(
        "ERROR: Failed to set curl write callback (client {}, "
        "port "
        "{})!",
        cli_addr, cli_port);
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to set "
        "callback write function</p></html>";
    return;
  }
  pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &body);
  if (pma_curl_ret != CURLE_OK) {
    PMA_EPrintln(
        "ERROR: Failed to set curl write callback user-data "
        "(client {}, port {})!",
        cli_addr, cli_port);
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to set "
        "callback write function user-data</p></html>";
    return;
  }

  // Set callback for fetched headers
  std::unordered_map<std::string, std::string> resp_headers;
  pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION,
                                  pma_curl_header_callback);
  if (pma_curl_ret != CURLE_OK) {
    PMA_EPrintln(
        "ERROR: Failed to set header callback (client {}, port "
        "{})!",
        cli_addr, cli_port);
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to set "
        "curl header callback</p></html>";
    return;
  }

  pma_curl_ret =
      curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &resp_headers);
  if (pma_curl_ret != CURLE_OK) {
    PMA_EPrintln(
        "ERROR: Failed to set header callback user-data (client "
        "{}, port {})!",
        cli_addr, cli_port);
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to set "
        "curl header callback user-data</p></html>";
    return;
  }

  // Set callback for sending data
  void **ptrs = reinterpret_cast<void **>(std::malloc(sizeof(void *) * 2));
  GenericCleanup<void ***> ptrs_cleanup(
      &ptrs, [](void ****ptrs) { std::free(**ptrs); });
  size_t count = 0;
  ptrs[0] = const_cast<std::string *>(&req.body);
  ptrs[1] = &count;
  if (!req.body.empty()) {
#ifndef NDEBUG
    PMA_Println("NOTICE: Sending client {} request body...", cli_addr);
#endif
    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl upload as POST (client {}, "
          "port {})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to "
          "set "
          "curl upload as POST</ p> < / html > ";
      return;
    }

    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, nullptr);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl upload as POST (fields; "
          "client {}, "
          "port {})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to "
          "set "
          "curl upload as POST (fields)</ p> < / html > ";
      return;
    }

    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION,
                                    pma_curl_body_send_callback);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl upload callback (client {}, "
          "port {})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to "
          "set "
          "curl upload callback</p></html>";
      return;
    }

    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_READDATA, ptrs);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl upload callback user-data "
          "(client {}, port {})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to "
          "set "
          "curl upload callback user-data</p></html>";
      return;
    }

    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE_LARGE,
                                    req.body.size());
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl POST size (client {}, port "
          "{})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to "
          "set "
          "curl POST size</p></html>";
      return;
    }
  }

  // Fetch
  pma_curl_ret = curl_easy_perform(curl_handle);
  if (pma_curl_ret != CURLE_OK) {
    PMA_EPrintln("ERROR: Failed to fetch with curl (client {}, port {})!",
                 cli_addr, cli_port);
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to "
        "fetch "
        "with curl</p></html>";
    return;
  }

  long resp_code = 200;

  pma_curl_ret =
      curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &resp_code);
  if (pma_curl_ret != CURLE_OK) {
    PMA_EPrintln(
        "ERROR: Failed to get curl fetch response code (client "
        "{}, "
        "port {})!",
        cli_addr, cli_port);
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to get "
        "curl fetch response code</p></html>";
    return;
  }

  switch (resp_code) {
    case 200:
      status = "HTTP/1.0 200 OK";
      break;
    case 400:
      status = "HTTP/1.0 400 Bad Request";
      break;
    case 401:
      status = "HTTP/1.0 401 Unauthorized";
      break;
    case 403:
      status = "HTTP/1.0 403 Forbidden";
      break;
    case 404:
      status = "HTTP/1.0 404 Not Found";
      break;
    case 502:
      status = "HTTP/1.0 502 Bad Gateway";
      break;
    case 503:
      status = "HTTP/1.0 503 Service Unavailable";
      break;
    case 504:
      status = "HTTP/1.0 504 Gateway Timeout";
      break;
    default:
      PMA_EPrintln("WARNING: Unhandled response code {} for client {}",
                   resp_code, cli_addr);
      [[fallthrough]];
    case 500:
      status = "HTTP/1.0 500 Internal Server Error";
      break;
  }

  // DEBUG
  // PMA_Println("Result headers:");
  // for (auto header_iter = resp_headers.begin();
  //      header_iter != resp_headers.end(); ++header_iter) {
  //   PMA_Println("  {}: {}", header_iter->first,
  //               header_iter->second);
  // }
  // PMA_Println("Result data: {}", body);

  content_type.clear();
  for (auto header_iter = resp_headers.begin();
       header_iter != resp_headers.end(); ++header_iter) {
    if (header_iter->first == "content-length" ||
        header_iter->first == "transfer-encoding") {
      continue;
    }
    content_type.append(
        std::format("{}: {}\r\n", header_iter->first, header_iter->second));
  }
  content_type.resize(content_type.size() - 2);
}

int main(int argc, char **argv) {
  const PMA_ARGS::Args args(argc, argv);

  if (args.flags.test(2)) {
    PMA_EPrintln("ERROR: Failed to parse args!");
    return 3;
  }

  std::optional<PMA_MSQL::Conf> msql_conf_opt;
  std::optional<PMA_MSQL::Connection> msql_conn_opt;
  if (args.flags.test(4)) {
    msql_conf_opt = PMA_MSQL::parse_conf_file(args.mysql_conf_path);
    if (!msql_conf_opt.has_value()) {
      PMA_EPrintln("ERROR: Failed to get MSQL opts for connection!");
      return 5;
    }
    msql_conn_opt = PMA_MSQL::Connection::connect_msql(
        msql_conf_opt->addr, msql_conf_opt->port, msql_conf_opt->user,
        msql_conf_opt->pass, msql_conf_opt->db);
    if (!msql_conn_opt.has_value() || !msql_conn_opt->ping_check()) {
      PMA_EPrintln("ERROR: Failed to connect to MSQL!");
      return 6;
    }

    PMA_MSQL::init_db(msql_conn_opt.value());
  }

  curl_global_init(CURL_GLOBAL_SSL);

  // Mapping is a socket-fd to AddrPortInfo
  std::unordered_map<int, AddrPortInfo> sockets;
  GenericCleanup<std::unordered_map<int, AddrPortInfo> *> cleanup_sockets(
      &sockets, [](std::unordered_map<int, AddrPortInfo> **s) {
        PMA_Println("Cleaning up sockets...");
        for (auto iter = (*s)->begin(); iter != (*s)->end(); ++iter) {
          if (iter->first >= 0) {
            close(iter->first);
          }
        }
      });

  for (const PMA_ARGS::AddrPort &a : args.addr_ports) {
    std::optional<int> socket_fd_opt;
    bool is_ipv4;
    const auto [err, msg_v6, socket_fd] =
        PMA_HTTP::get_ipv6_socket_server(std::get<0>(a), std::get<1>(a));
    if (err == PMA_HTTP::ErrorT::SUCCESS) {
      socket_fd_opt = socket_fd;
      is_ipv4 = false;
    } else {
      const auto [err, msg, socket_fd] =
          PMA_HTTP::get_ipv4_socket_server(std::get<0>(a), std::get<1>(a));
      if (err == PMA_HTTP::ErrorT::SUCCESS) {
        socket_fd_opt = socket_fd;
        is_ipv4 = true;
      } else {
        PMA_EPrintln(
            "ERROR: Failed to get listening socket for addr \"{}\" on port "
            "\"{}\" (ipv6: {}, ipv4: {})!",
            std::get<0>(a), std::get<1>(a), msg_v6, msg);
        return 1;
      }
    }

    if (socket_fd_opt.has_value() && socket_fd_opt.value() >= 0) {
      sockets.emplace(socket_fd_opt.value(), conv_addr_port(a, is_ipv4));
      PMA_Println("Listening on {}:{}", std::get<0>(a), std::get<1>(a));
    } else {
      PMA_EPrintln(
          "ERROR: Invalid internal state with addr \"{}\" and port \"{}\"!",
          std::get<0>(a), std::get<1>(a));
      return 2;
    }
  }

  if (sockets.empty()) {
    PMA_EPrintln("ERROR: Not listening to any sockets!");
    return 4;
  }

  // Mapping is a connection-fd to AddrPortInfo of host/server
  std::unordered_map<int, AddrPortInfo> connections;
  GenericCleanup<std::unordered_map<int, AddrPortInfo> *> cleanup_connections(
      &connections, [](std::unordered_map<int, AddrPortInfo> **s) {
        PMA_Println("Cleaning up connections...");
        for (auto iter = (*s)->begin(); iter != (*s)->end(); ++iter) {
          if (iter->first >= 0) {
            close(iter->first);
          }
        }
      });

  PMA_HELPER::set_signal_handler(SIGINT, receive_signal);
  PMA_HELPER::set_signal_handler(SIGHUP, receive_signal);
  PMA_HELPER::set_signal_handler(SIGTERM, receive_signal);

  struct sockaddr_in sain4;
  std::memset(&sain4, 0, sizeof(struct sockaddr_in));
  struct sockaddr_in6 sain6;
  std::memset(&sain6, 0, sizeof(struct sockaddr_in6));
  socklen_t sain_len;

  std::deque<int> to_remove_connections;
  std::array<char, REQ_READ_BUF_SIZE> buf;

  std::unordered_map<std::string,
                     std::chrono::time_point<std::chrono::steady_clock> >
      cached_allowed;
  std::chrono::time_point<std::chrono::steady_clock> time_now =
      std::chrono::steady_clock::now();
  std::chrono::time_point<std::chrono::steady_clock> time_prev = time_now;

  int ret;
  const auto sleep_duration = std::chrono::milliseconds(SLEEP_MILLISECONDS);
  while (!interrupt_received) {
    std::this_thread::sleep_for(sleep_duration);
    time_now = std::chrono::steady_clock::now();
    if (time_now - time_prev > CACHED_CLEAR_T) {
      time_prev = time_now;
      cached_allowed.clear();
    }

    // Fetch new connections
    for (auto iter = sockets.begin(); iter != sockets.end(); ++iter) {
      ret = 0;
      while (ret >= 0) {
        if (iter->second.flags.test(0)) {
          // IPV4
          sain_len = sizeof(struct sockaddr_in);
          ret = accept(iter->first, reinterpret_cast<sockaddr *>(&sain4),
                       &sain_len);

          if (sain_len != sizeof(struct sockaddr_in)) {
            PMA_EPrintln("WARNING: sockaddr return length {}, but should be {}",
                         sain_len, sizeof(struct sockaddr_in));
          }
        } else {
          // IPV6
          sain_len = sizeof(struct sockaddr_in6);
          ret = accept(iter->first, reinterpret_cast<sockaddr *>(&sain6),
                       &sain_len);

          if (sain_len != sizeof(struct sockaddr_in6)) {
            PMA_EPrintln("WARNING: sockaddr return length {}, but should be {}",
                         sain_len, sizeof(struct sockaddr_in6));
          }
        }

        if (ret == -1) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Nonblocking-IO indicating no connection to accept
          } else {
            PMA_EPrintln(
                "WARNING: Failed to accept connection from socket {} (errno "
                "{})",
                iter->first, errno);
          }
        } else if (iter->second.flags.test(0)) {
          // IPV4 new connection
          std::string client_ipv4 =
              PMA_HTTP::ipv4_addr_to_str(sain4.sin_addr.s_addr);
#ifndef NDEBUG
          PMA_Println("New connection from {}:{} on port {}", client_ipv4,
                      PMA_HELPER::be_swap_u16(sain4.sin_port),
                      iter->second.port);
#endif

          // Set nonblocking-IO on received connection fd
          int fcntl_ret = fcntl(ret, F_SETFL, O_NONBLOCK);
          if (fcntl_ret < 0) {
            PMA_EPrintln(
                "WARNING: Failed to set NONBLOCK on new connection (errno {}), "
                "closing connection...",
                errno);
            close(ret);
          }

          iter->second.client_addr = std::move(client_ipv4);
          connections.emplace(ret, iter->second);
        } else {
          // IPV6 new connection
          std::string client_ipv6 = PMA_HTTP::ipv6_addr_to_str(
              *reinterpret_cast<std::array<uint8_t, 16> *>(
                  sain6.sin6_addr.s6_addr));
#ifndef NDEBUG
          PMA_Println("New connection from {}:{} on port {}", client_ipv6,
                      PMA_HELPER::be_swap_u16(sain6.sin6_port),
                      iter->second.port);
#endif

          // Set nonblocking-IO on received connection fd
          int fcntl_ret = fcntl(ret, F_SETFL, O_NONBLOCK);
          if (fcntl_ret < 0) {
            PMA_EPrintln(
                "WARNING: Failed to set NONBLOCK on new connection (errno {}), "
                "closing connection...",
                errno);
            close(ret);
          }

          iter->second.client_addr = client_ipv6;
          connections.emplace(ret, iter->second);
        }
      }  // while (ret >= 0)
    }  // for (sockets ... )

    // Handle connections
    for (auto iter = connections.begin(); iter != connections.end(); ++iter) {
      iter->second.ticks += 1;
      if (iter->second.ticks >= TIMEOUT_ITER_TICKS) {
#ifndef NDEBUG
        PMA_Println("Timed out connection from {} on port {}",
                    iter->second.client_addr, iter->second.port);
#endif
        to_remove_connections.push_back(iter->first);
        continue;
      }

      if (iter->second.remaining_buffer.has_value()) {
        ssize_t write_ret =
            write(iter->first, iter->second.remaining_buffer.value().data(),
                  iter->second.remaining_buffer.value().size());
        if (write_ret == -1) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Nonblocking-IO indicating to write later, intentionally left
            // blank.
          } else {
            PMA_EPrintln(
                "ERROR: Failed to send partial response to client {} "
                "(write_ret {}, errno {})!",
                iter->second.client_addr, write_ret, errno);
            to_remove_connections.push_back(iter->first);
          }
        } else if (write_ret !=
                   static_cast<ssize_t>(
                       iter->second.remaining_buffer.value().size())) {
          if (write_ret > 0) {
            iter->second.remaining_buffer =
                iter->second.remaining_buffer.value().substr(
                    static_cast<size_t>(write_ret));
          } else {
            PMA_EPrintln(
                "ERROR: Failed to send partial response to client {} "
                "(write_ret {}, errno {})!",
                iter->second.client_addr, write_ret, errno);
            to_remove_connections.push_back(iter->first);
          }
        } else {
          iter->second.remaining_buffer = std::nullopt;
          iter->second.ticks = 0;
        }
        continue;
      }

      ssize_t read_ret = read(iter->first, buf.data(), buf.size() - 1);
      if (read_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // Nonblocking-IO indicating no bytes to read
          continue;
        } else {
          PMA_Println("Failed to read from client {} (errno {})",
                      iter->second.client_addr, errno);
          to_remove_connections.push_back(iter->first);
          continue;
        }
      }
      if (read_ret > 0) {
        iter->second.ticks = 0;
        buf.at(static_cast<size_t>(read_ret)) = 0;
        PMA_HTTP::Request req = PMA_HTTP::handle_request_parse(
            std::string(buf.data(), static_cast<size_t>(read_ret)));
        if (req.error_enum == PMA_HTTP::ErrorT::SUCCESS) {
#ifndef NDEBUG
          PMA_Println("URL: {}, FULL URL: {}, Params:", req.url_or_err_msg,
                      req.full_url);
          for (auto qiter = req.queries.begin(); qiter != req.queries.end();
               ++qiter) {
            PMA_Println("  {}={}", qiter->first, qiter->second);
          }
          PMA_Println("Headers:");
          for (auto hiter = req.headers.begin(); hiter != req.headers.end();
               ++hiter) {
            PMA_Println("  {}: {}", hiter->first, hiter->second);
          }
#endif
          if (args.flags.test(0)) {
            if (auto fiter = req.headers.find("x-real-ip");
                fiter != req.headers.end()) {
#ifndef NDEBUG
              PMA_Println("x-real-ip header found, changing client addr: {}",
                          fiter->second);
#endif
              iter->second.client_addr = fiter->second;
            }
          }

          std::string status = "HTTP/1.0 200 OK";
          std::string content_type = "Content-type: text/html; charset=utf-8";
          std::string body;
          PMA_SQL::SQLITECtx sqliteCtx;
          PMA_SQL::ErrorT err = PMA_SQL::ErrorT::SUCCESS;
          std::string msg;
          if (!args.flags.test(4)) {
            std::tie(sqliteCtx, err, msg) =
                PMA_SQL::init_sqlite(args.sqlite_path);
          }
          if (err != PMA_SQL::ErrorT::SUCCESS) {
            PMA_EPrintln("ERROR: Failed to initialize sqlite: {}, {}",
                         PMA_SQL::error_t_to_string(err), msg);
            status = "HTTP/1.0 500 Internal Server Error";
            body =
                "<html><p>500 Internal Server Error</p><p>Failed to init "
                "db</p></html>";
          } else if (req.url_or_err_msg == args.api_url) {
            const auto [err, json_keyvals] =
                PMA_HTTP::parse_simple_json(req.body);
            if (err != PMA_HTTP::ErrorT::SUCCESS) {
              PMA_EPrintln("ERROR: Failed to parse json from client {}!",
                           iter->second.client_addr);
              status = "HTTP/1.0 500 Internal Server Error";
              body =
                  "<html><p>500 Internal Server Error</p><p>Failed to parse "
                  "json</p></html>";
            } else if (json_keyvals.find("type") == json_keyvals.end() ||
                       json_keyvals.find("id") == json_keyvals.end() ||
                       json_keyvals.find("factors") == json_keyvals.end()) {
              PMA_EPrintln("ERROR: Client {} omitted necessary info!",
                           iter->second.client_addr);
              status = "HTTP/1.0 400 Bad Request";
              body = "<html><p>400 Bad Request</p><p>Missing info</p></html>";
            } else if (args.flags.test(4)) {
              bool ping_ok = false;
              if (!msql_conn_opt.has_value() || !msql_conn_opt->ping_check()) {
                msql_conn_opt = PMA_MSQL::Connection::connect_msql(
                    msql_conf_opt->addr, msql_conf_opt->port,
                    msql_conf_opt->user, msql_conf_opt->pass,
                    msql_conf_opt->db);
                if (!msql_conn_opt.has_value() ||
                    !msql_conn_opt->ping_check()) {
                  PMA_EPrintln("ERROR: Connection to MSQL server lost!");
                  status = "HTTP/1.0 500 Internal Server Error";
                  body =
                      "<html><p>500 Internal Server Error</p><p>Problem with "
                      "DB</p></html>";
                } else {
                  ping_ok = true;
                }
              } else {
                ping_ok = true;
              }
              if (ping_ok) {
                const auto [err, port] = PMA_MSQL::validate_client(
                    msql_conn_opt.value(), args.challenge_timeout,
                    json_keyvals.find("id")->second,
                    json_keyvals.find("factors")->second,
                    iter->second.client_addr);
                if (err == PMA_MSQL::Error::SUCCESS) {
                  PMA_Println("Challenge success from {} port {}",
                              iter->second.client_addr, iter->second.port);
                  content_type = "Content-type: text/plain";
                  body = "Correct";
                  cached_allowed.insert(std::make_pair(
                      std::format("{}:{}", iter->second.client_addr,
                                  iter->second.port),
                      time_now));
                } else {
                  PMA_EPrintln(
                      "Warning: Failed to validate client {}:{} due to {}",
                      iter->second.client_addr, iter->second.port,
                      PMA_MSQL::error_to_str(err));
                  if (PMA_MSQL::error_is_client_err(err)) {
                    status = "HTTP/1.0 400 Bad Request";
                    content_type = "Content-type: text/plain";
                    body = "Incorrect";
                  } else {
                    status = "HTTP/1.0 500 Internal Server Error";
                    body =
                        "<html><p>500 Internal Server Error</p><p>Failed to "
                        "validate req</p></html>";
                  }
                }
              }
            } else {
              const auto [err, msg, port] = PMA_SQL::verify_answer(
                  sqliteCtx, json_keyvals.find("factors")->second,
                  iter->second.client_addr, json_keyvals.find("id")->second);
              if (err != PMA_SQL::ErrorT::SUCCESS) {
                PMA_EPrintln("ERROR: Challenge failed from {}! {}, {}",
                             iter->second.client_addr,
                             PMA_SQL::error_t_to_string(err), msg);
                status = "HTTP/1.0 400 Bad Request";
                content_type = "Content-type: text/plain";
                body = "Incorrect";
              } else {
                PMA_Println("Challenge success from {} port {}",
                            iter->second.client_addr, iter->second.port);
                content_type = "Content-type: text/plain";
                body = "Correct";
                cached_allowed.insert(std::make_pair(
                    std::format("{}:{}", iter->second.client_addr,
                                iter->second.port),
                    time_now));
              }
            }

          } else if (req.url_or_err_msg == args.js_factors_url) {
            if (auto id_iter = req.queries.find("id");
                id_iter != req.queries.end()) {
              if (args.flags.test(4)) {
                bool ping_ok = false;
                if (!msql_conn_opt.has_value() ||
                    !msql_conn_opt->ping_check()) {
                  msql_conn_opt = PMA_MSQL::Connection::connect_msql(
                      msql_conf_opt->addr, msql_conf_opt->port,
                      msql_conf_opt->user, msql_conf_opt->pass,
                      msql_conf_opt->db);
                  if (!msql_conn_opt.has_value() ||
                      !msql_conn_opt->ping_check()) {
                    PMA_EPrintln("ERROR: Connection to MSQL server lost!");
                    status = "HTTP/1.0 500 Internal Server Error";
                    body =
                        "<html><p>500 Internal Server Error</p><p>Problem with "
                        "DB</p></html>";
                  } else {
                    ping_ok = true;
                  }
                } else {
                  ping_ok = true;
                }
                if (ping_ok) {
                  const auto [itp_err, port] = PMA_MSQL::get_id_to_port_port(
                      msql_conn_opt.value(), id_iter->second);
                  if (itp_err == PMA_MSQL::Error::SUCCESS) {
                    const auto [cf_err, chall, hashed_id] =
                        PMA_MSQL::set_challenge_factor(
                            msql_conn_opt.value(), iter->second.client_addr,
                            port, args.factors, args.challenge_timeout);
                    if (cf_err == PMA_MSQL::Error::SUCCESS) {
                      body = JS_FACTORS_WORKER;
                      PMA_HELPER::str_replace_all(body, "{API_URL}",
                                                  args.api_url);
                      PMA_HELPER::str_replace_all(body, "{LARGE_NUMBER}",
                                                  chall);
                      PMA_HELPER::str_replace_all(body, "{UUID}", hashed_id);
                      content_type = "Content-type: text/javascript";
                    } else {
                      if (PMA_MSQL::error_is_client_err(cf_err)) {
                        status = "HTTP/1.0 400 Bad Request";
                        body =
                            "<html><p>400 Bad Request</p><p>(Failed setup "
                            "challenge)</p></html>";
                      } else {
                        status = "HTTP/1.0 500 Internal Server Error";
                        body =
                            "<html><p>500 Internal Server Error</p><p>Failed "
                            "to set up challenge</p></html>";
                      }
                    }
                  } else {
                    if (PMA_MSQL::error_is_client_err(itp_err)) {
                      PMA_EPrintln(
                          "Error: Bad request from client {}:{} due to {}",
                          iter->second.client_addr, iter->second.port,
                          PMA_MSQL::error_to_str(itp_err));
                      status = "HTTP/1.0 400 Bad Request";
                      body =
                          "<html><p>400 Bad Request</p><p>(No id)</p></html>";
                    } else {
                      PMA_EPrintln("Error: handling client {}:{} due to {}",
                                   iter->second.client_addr, iter->second.port,
                                   PMA_MSQL::error_to_str(itp_err));
                      status = "HTTP/1.0 500 Internal Server Error";
                      body =
                          "<html><p>500 Internal Server Error</p><p>Failed to "
                          "set up challenge</p></html>";
                    }
                  }
                }
              } else {
                PMA_SQL::cleanup_stale_challenges(sqliteCtx,
                                                  args.challenge_timeout);
                const auto [err, msg_or_chal, answ, id] =
                    PMA_SQL::generate_challenge(sqliteCtx, args.factors,
                                                iter->second.client_addr,
                                                id_iter->second);
                if (err != PMA_SQL::ErrorT::SUCCESS) {
                  PMA_EPrintln(
                      "ERROR: Failed to prepare challenge for client {}: {}, "
                      "{}",
                      iter->second.client_addr, PMA_SQL::error_t_to_string(err),
                      msg_or_chal);
                  status = "HTTP/1.0 500 Internal Server Error";
                  body =
                      "<html><p>500 Internal Server Error</p><p>Failed to "
                      "prepare challenge</p></html>";
                } else {
                  body = JS_FACTORS_WORKER;
                  PMA_HELPER::str_replace_all(body, "{API_URL}", args.api_url);
                  PMA_HELPER::str_replace_all(body, "{LARGE_NUMBER}",
                                              msg_or_chal);
                  PMA_HELPER::str_replace_all(body, "{UUID}", id);
                  content_type = "Content-type: text/javascript";
                }
              }
            } else {
              status = "HTTP/1.0 400 Bad Request";
              body = "<html><p>400 Bad Request</p><p>(No id)</p></html>";
            }
          } else if (args.flags.test(4)) {
            if (auto cached_iter = cached_allowed.find(std::format(
                    "{}:{}", iter->second.client_addr, iter->second.port));
                cached_iter != cached_allowed.end()) {
              if (time_now - cached_iter->second > CACHED_TIMEOUT_T) {
                cached_allowed.erase(cached_iter);
              } else {
                do_curl_forwarding(iter->second.client_addr, iter->second.port,
                                   body, status, content_type, req, args);
                goto PMA_RESPONSE_SEND_LOCATION;
              }
            }

            bool ping_ok = false;
            if (!msql_conn_opt.has_value() || !msql_conn_opt->ping_check()) {
              msql_conn_opt = PMA_MSQL::Connection::connect_msql(
                  msql_conf_opt->addr, msql_conf_opt->port, msql_conf_opt->user,
                  msql_conf_opt->pass, msql_conf_opt->db);
              if (!msql_conn_opt.has_value() || !msql_conn_opt->ping_check()) {
                PMA_EPrintln("ERROR: Connection to MSQL server lost!");
                status = "HTTP/1.0 500 Internal Server Error";
                body =
                    "<html><p>500 Internal Server Error</p><p>Problem with "
                    "DB</p></html>";
              } else {
                ping_ok = true;
              }
            } else {
              ping_ok = true;
            }

            if (ping_ok) {
              PMA_MSQL::Error is_allowed_e = PMA_MSQL::client_is_allowed(
                  msql_conn_opt.value(), iter->second.client_addr,
                  iter->second.port, args.allowed_timeout);
              if (is_allowed_e == PMA_MSQL::Error::SUCCESS) {
                cached_allowed.insert(std::make_pair(
                    std::format("{}:{}", iter->second.client_addr,
                                iter->second.port),
                    time_now));
                do_curl_forwarding(iter->second.client_addr, iter->second.port,
                                   body, status, content_type, req, args);
                goto PMA_RESPONSE_SEND_LOCATION;
              } else if (is_allowed_e == PMA_MSQL::Error::EMPTY_QUERY_RESULT) {
                const auto [err, id] = PMA_MSQL::init_id_to_port(
                    msql_conn_opt.value(), iter->second.port,
                    args.challenge_timeout);
                if (err == PMA_MSQL::Error::SUCCESS) {
                  body = HTML_BODY_FACTORS;
                  PMA_HELPER::str_replace_all(
                      body, "{JS_FACTORS_URL}",
                      std::format("{}?id={}", args.js_factors_url, id));
                } else {
                  PMA_EPrintln(
                      "ERROR: Failed to init id-to-port for client {}! {}",
                      iter->second.client_addr, PMA_MSQL::error_to_str(err));
                  if (PMA_MSQL::error_is_client_err(err)) {
                    status = "HTTP/1.0 400 Bad Request";
                    body = "<html><p>400 Bad Request</p></html>";
                  } else {
                    status = "HTTP/1.0 500 Internal Server Error";
                    body =
                        "<html><p>500 Internal Server Error</p><p>Failed "
                        "prepare for client</p></html>";
                  }
                }
              } else {
                PMA_EPrintln(
                    "ERROR: Failed to check if client {} is allowed: {}",
                    iter->second.client_addr,
                    PMA_MSQL::error_to_str(is_allowed_e));
                if (PMA_MSQL::error_is_client_err(is_allowed_e)) {
                  status = "HTTP/1.0 400 Bad Request";
                  body = "<html><p>400 Bad Request</p></html>";
                } else {
                  status = "HTTP/1.0 500 Internal Server Error";
                  body =
                      "<html><p>500 Internal Server Error</p><p>Failed to "
                      "check client</p></html>";
                }
              }
            }
          } else {
            if (auto cached_iter = cached_allowed.find(std::format(
                    "{}:{}", iter->second.client_addr, iter->second.port));
                cached_iter != cached_allowed.end()) {
              if (time_now - cached_iter->second > CACHED_TIMEOUT_T) {
                cached_allowed.erase(cached_iter);
              } else {
                do_curl_forwarding(iter->second.client_addr, iter->second.port,
                                   body, status, content_type, req, args);
                goto PMA_RESPONSE_SEND_LOCATION;
              }
            }

            PMA_SQL::cleanup_stale_entries(sqliteCtx, args.allowed_timeout);

            const auto [err, msg, is_allowed] = PMA_SQL::is_allowed_ip_port(
                sqliteCtx, iter->second.client_addr, iter->second.port);
            if (err != PMA_SQL::ErrorT::SUCCESS || !is_allowed) {
              PMA_SQL::cleanup_stale_id_to_ports(sqliteCtx,
                                                 args.challenge_timeout);
              const auto [err, msg, id] =
                  PMA_SQL::init_id_to_port(sqliteCtx, iter->second.port);
              body = HTML_BODY_FACTORS;
              PMA_HELPER::str_replace_all(
                  body, "{JS_FACTORS_URL}",
                  std::format("{}?id={}", args.js_factors_url, id));
            } else {
              cached_allowed.insert(
                  std::make_pair(std::format("{}:{}", iter->second.client_addr,
                                             iter->second.port),
                                 time_now));
              do_curl_forwarding(iter->second.client_addr, iter->second.port,
                                 body, status, content_type, req, args);
              goto PMA_RESPONSE_SEND_LOCATION;
            }
          }

        PMA_RESPONSE_SEND_LOCATION:
          std::string full =
              std::format("{}\r\n{}\r\nContent-Length: {}\r\n\r\n{}", status,
                          content_type, body.size(), body);
          ssize_t write_ret = write(iter->first, full.data(), full.size());
          if (write_ret != static_cast<ssize_t>(full.size())) {
            if (write_ret > 0) {
              iter->second.remaining_buffer =
                  full.substr(static_cast<size_t>(write_ret));
            } else {
              PMA_EPrintln(
                  "ERROR: Failed to send response to client {} (write_ret {})!",
                  iter->second.client_addr, write_ret);
              to_remove_connections.push_back(iter->first);
            }
          } else if (write_ret == -1) {
            PMA_EPrintln("ERROR: Failed to write to client {}, errno {}!",
                         iter->second.client_addr, errno);
            to_remove_connections.push_back(iter->first);
          } else {
            // Success, intentionally left blank.
          }
        } else {
          PMA_EPrintln("ERROR {}: {}", PMA_HTTP::error_t_to_str(req.error_enum),
                       req.url_or_err_msg);
          to_remove_connections.push_back(iter->first);
        }
      } else if (read_ret == 0) {
#ifndef NDEBUG
        PMA_Println("EOF From client {} (port {}), closing...",
                    iter->second.client_addr, iter->second.port);
#endif
        to_remove_connections.push_back(iter->first);
      }
    }

    // Remove connections
    for (int connection_fd : to_remove_connections) {
      close(connection_fd);
      connections.erase(connection_fd);
    }
    to_remove_connections.clear();

    std::cout << std::flush;
  }

  return 0;
}

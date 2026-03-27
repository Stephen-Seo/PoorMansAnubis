// ISC License
//
// Copyright (c) 2025-2026 Stephen Seo
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
#include "thread_pool.h"

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
  uint16_t remote_port;
  uint16_t local_port;
  uint16_t ticks;
};

AddrPortInfo conv_addr_port(const PMA_ARGS::AddrPort &addr_port, bool is_ipv4) {
  AddrPortInfo info = {std::nullopt,
                       std::get<0>(addr_port),
                       std::string{},
                       std::bitset<16>{},
                       0,
                       std::get<1>(addr_port),
                       0};

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
  if (req.method == "GET") {
#ifndef NDEBUG
    PMA_Println("NOTICE: Sending client {} GET request...", cli_addr);
#endif
  } else if (req.method == "POST") {
#ifndef NDEBUG
    PMA_Println("NOTICE: Sending client {} POST request body...", cli_addr);
#endif
    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln("ERROR: Failed to set curl POST (client {}, port {})!",
                   cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to "
          "set "
          "curl upload as POST</p> </html> ";
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
          "curl upload as POST (fields)</p> </html> ";
      return;
    }

    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION,
                                    pma_curl_body_send_callback);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl POST READFUNCTION (client {}, "
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
          "ERROR: Failed to set curl POST READDATA (client {}, port {})!",
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
  } else if (req.method == "PUT") {
#ifndef NDEBUG
    PMA_Println("NOTICE: Sending client {} PUT request...", cli_addr);
#endif
    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln("ERROR: Failed to set curl PUT (client {}, port {})!",
                   cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set curl request "
          "as PUT</p> </html> ";
      return;
    }

    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION,
                                    pma_curl_body_send_callback);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl PUT READFUNCTION (client {}, port {})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set up curl</p> "
          "</html> ";
      return;
    }

    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_READDATA, ptrs);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl PUT READDATA (client {}, port {})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set up curl</p> "
          "</html> ";
      return;
    }

    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE_LARGE,
                                    req.body.size());
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl PUT INFILESIZE_LARGE (client {}, port "
          "{})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set up curl</p> "
          "</html> ";
      return;
    }
  } else if (req.method == "HEAD") {
#ifndef NDEBUG
    PMA_Println("NOTICE: Sending client {} HEAD request...", cli_addr);
#endif
    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "HEAD");
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln("ERROR: Failed to set curl HEAD (client {}, port {})!",
                   cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set curl request "
          "as HEAD</p> </html> ";
      return;
    }
  } else if (req.method == "DELETE") {
#ifndef NDEBUG
    PMA_Println("NOTICE: Sending client {} DELETE request...", cli_addr);
#endif
    pma_curl_ret =
        curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "DELETE");
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln("ERROR: Failed to set curl DELETE (client {}, port {})!",
                   cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set curl "
          "request "
          "as DELETE</p> </html> ";
      return;
    }
  } else if (req.method == "OPTIONS") {
#ifndef NDEBUG
    PMA_Println("NOTICE: Sending client {} OPTIONS request...", cli_addr);
#endif
    pma_curl_ret =
        curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "OPTIONS");
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln("ERROR: Failed to set curl OPTIONS (client {}, port {})!",
                   cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set curl "
          "request "
          "as OPTIONS</p> </html> ";
      return;
    }
  } else if (req.method == "TRACE") {
#ifndef NDEBUG
    PMA_Println("NOTICE: Sending client {} TRACE request...", cli_addr);
#endif
    pma_curl_ret =
        curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "TRACE");
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln("ERROR: Failed to set curl TRACE (client {}, port {})!",
                   cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set curl "
          "request "
          "as TRACE</p> </html> ";
      return;
    }
  } else if (req.method == "PATCH") {
#ifndef NDEBUG
    PMA_Println("NOTICE: Sending client {} PATCH request...", cli_addr);
#endif
    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln("ERROR: Failed to set curl PATCH (client {}, port {})!",
                   cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set curl "
          "request "
          "as PATCH</p> </html> ";
      return;
    }

    pma_curl_ret =
        curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "PATCH");

    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION,
                                    pma_curl_body_send_callback);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl PATCH READFUNCTION (client {}, port "
          "{})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set up "
          "curl</p> </html> ";
      return;
    }

    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_READDATA, ptrs);
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl PATCH READDATA (client {}, port {})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set up "
          "curl</p> </html> ";
      return;
    }

    pma_curl_ret = curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE_LARGE,
                                    req.body.size());
    if (pma_curl_ret != CURLE_OK) {
      PMA_EPrintln(
          "ERROR: Failed to set curl PATCH INFILESIZE_LARGE (client {}, port "
          "{})!",
          cli_addr, cli_port);
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Failed to set up "
          "curl</p> </html> ";
      return;
    }
  } else {
    PMA_EPrintln("ERROR: Invalid request method {}!", req.method);
    status = "HTTP/1.0 400 Bad Request";
    body = std::format(
        "<html><p>400 Bad Request</p><p>{} is not supported</p> </html> ",
        req.method);
    return;
  }

  // Fetch
  pma_curl_ret = curl_easy_perform(curl_handle);
  if (pma_curl_ret != CURLE_OK) {
    PMA_EPrintln("ERROR: Failed to fetch with curl (client {}, port {})!",
                 cli_addr, cli_port);
    long error = 0;
    pma_curl_ret = curl_easy_getinfo(curl_handle, CURLINFO_OS_ERRNO, &error);
    if (pma_curl_ret == CURLE_OK) {
      PMA_EPrintln("Errno: {}", error);
    }
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to fetch with "
        "curl</p></html>";
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

void do_ipv4_socket_forwarding(std::string cli_addr, uint16_t cli_port,
                               std::string &body, std::string &status,
                               std::string &content_type,
                               std::bitset<32> &forward_flags,
                               const PMA_HTTP::Request &req,
                               const PMA_ARGS::Args &args) {
  std::string addr;
  uint32_t port = 80;

  {
    auto iter = args.port_to_dest_urls.find(cli_port);
    std::string full_addr;
    if (iter != args.port_to_dest_urls.end()) {
      full_addr = iter->second;
    } else {
      full_addr = args.default_dest_url;
    }
    std::optional<size_t> http_idx;
    for (size_t idx = 0; idx + 7 < full_addr.size(); ++idx) {
      if (std::strncmp("http://", full_addr.data() + idx, 7) == 0) {
        http_idx = idx;
        break;
      }
    }

    if (http_idx.has_value()) {
      size_t end_idx = http_idx.value() + 7;
      size_t decimal_count = 0;
      int_fast8_t has_number = 0;
      for (; end_idx < full_addr.size(); ++end_idx) {
        if (full_addr.at(end_idx) >= '0' && full_addr.at(end_idx) <= '9') {
          has_number = 1;
        } else if (full_addr.at(end_idx) == '.') {
          if (has_number == 0) {
            PMA_EPrintln(
                "ERROR: Failed to parse ip addr from url (invalid ipv4)!");
            status = "HTTP/1.0 500 Internal Server Error";
            body =
                "<html><p>500 Internal Server Error</p><p>Invalid "
                "settings</p></html>";
            return;
          }
          ++decimal_count;
          has_number = 0;
          if (decimal_count > 3) {
            PMA_EPrintln(
                "ERROR: Failed to parse ip addr from url (invalid ipv4, more "
                "than 3 decimals)!");
            status = "HTTP/1.0 500 Internal Server Error";
            body =
                "<html><p>500 Internal Server Error</p><p>Invalid "
                "settings</p></html>";
            return;
          }
        } else if (decimal_count == 3) {
          if (full_addr.at(end_idx) < '0' || full_addr.at(end_idx) > '9') {
            break;
          }
        }
      }

      if (has_number && decimal_count == 3) {
        addr = full_addr.substr(http_idx.value() + 7,
                                end_idx - (http_idx.value() + 7));
      } else {
        PMA_EPrintln(
            "ERROR: Failed to parse ip addr from url (failed to parse ipv4)!");
        status = "HTTP/1.0 500 Internal Server Error";
        body =
            "<html><p>500 Internal Server Error</p><p>Invalid "
            "settings</p></html>";
        return;
      }

      if (end_idx < full_addr.size() && full_addr.at(end_idx) == ':') {
        // Port is specified
        port = 0;
        for (size_t idx = end_idx + 1; idx < full_addr.size(); ++idx) {
          if (full_addr.at(idx) >= '0' && full_addr.at(idx) <= '9') {
            uint32_t digit = static_cast<uint16_t>(full_addr.at(idx) - '0');
            port = static_cast<uint32_t>(port * 10 + digit);
            if (port > 0xFFFF) {
              PMA_EPrintln(
                  "ERROR: Failed to parse ip addr from url (port is too "
                  "large)!");
              status = "HTTP/1.0 500 Internal Server Error";
              body =
                  "<html><p>500 Internal Server Error</p><p>Invalid "
                  "settings</p></html>";
              return;
            }
          } else {
            PMA_EPrintln(
                "ERROR: Failed to parse ip addr from url (invalid char while "
                "parsing port)!");
            status = "HTTP/1.0 500 Internal Server Error";
            body =
                "<html><p>500 Internal Server Error</p><p>Invalid "
                "settings</p></html>";
            return;
          }
        }
      }
    } else {
      PMA_EPrintln(
          "ERROR: Failed to parse ip addr from url (no \"http://\")! (For "
          "https://, use \"--enable-libcurl\")");
      status = "HTTP/1.0 500 Internal Server Error";
      body =
          "<html><p>500 Internal Server Error</p><p>Invalid "
          "settings</p></html>";
      return;
    }
  }

  if (addr.empty()) {
    PMA_EPrintln("ERROR: Failed to parse ip addr from url (failed to parse)!");
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Invalid settings</p></html>";
    return;
  }

  // PMA_EPrintln("DEBUG: Got addr: {}, port {}", addr, port);

  const auto [err, err_msg, socket_fd] = PMA_HTTP::connect_ipv4_socket_client(
      addr, "0.0.0.0", static_cast<uint16_t>(port));
  if (err != PMA_HTTP::ErrorT::SUCCESS) {
    PMA_EPrintln("ERROR: Failed to get socket (invalid addr/port?): {}",
                 err_msg);
    status = "HTTP/1.0 500 Internal Server Error";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to forward "
        "connect</p></html>";
    return;
  }

  GenericCleanup<int> cleanup_socket(socket_fd, [](int *fd) {
    if (fd && *fd && *fd > 0) {
      close(*fd);
      *fd = -1;
    }
  });

  {
    // write method
    std::string to_write = req.method;
    to_write.push_back(' ');
    size_t remaining = 0;

    // Write path
    to_write.append(req.full_url);
    to_write.append(" HTTP/1.1\r\n");

    // Write headers
    to_write.append(std::format("Host: {}:{}\r\n", addr, port));
    to_write.append(
        "Accept: text/html,application/xhtml+xml,application/xml,*/*\r\n");
    to_write.append("User-Agent: PoorMansAnubis\r\n");
    to_write.append("Connection: close\r\n");
    to_write.append(std::format("x-real-ip: {}\r\n", cli_addr));

    // End of headers
    to_write.append("\r\n");

    if (!req.body.empty()) {
      // Request content data
      to_write.append(req.body);
    }

    // PMA_EPrintln("DEBUG: to_write: {} END_to_write", to_write);

    remaining = to_write.size();

    size_t wait_ticks = 0;

    while (true) {
      ssize_t write_ret =
          write(socket_fd, to_write.data() + (to_write.size() - remaining),
                remaining);
      if (write_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          std::this_thread::sleep_for(SLEEP_MILLISECONDS_CHRONO);
          // PMA_EPrintln("DEGUG: write forwarding req: EAGAIN/EWOULDBLOCK");
          if (++wait_ticks > args.req_timeout_ticks) {
            PMA_EPrintln("ERROR: Failed to write to destination, errno {}",
                         errno);
            status = "HTTP/1.0 500 Internal Server Error";
            body =
                "<html><p>500 Internal Server Error</p><p>Failed to "
                "fetch, timed out write</p></html>";
            return;
          }
          continue;
        } else {
          PMA_EPrintln("ERROR: Failed to write to destination, errno {}",
                       errno);
          status = "HTTP/1.0 500 Internal Server Error";
          body =
              "<html><p>500 Internal Server Error</p><p>Failed to "
              "fetch</p></html>";
          return;
        }
      } else {
        remaining -= static_cast<size_t>(write_ret);
        if (remaining == 0) {
          break;
        }
        wait_ticks = 0;
      }
    }
  }

  std::array<char, REQ_READ_BUF_SIZE> buf;
  int_fast8_t before_first_line = 1;
  int_fast8_t before_content = 1;
  std::string temp;
  std::string header_name;
  std::string header_value;
  size_t skip_before_idx = 0;
  size_t wait_ticks = 0;
  std::optional<size_t> recv_content_size;

  body.clear();
  status.clear();
  content_type.clear();

  const auto verify_header_fn = [&header_name, &header_value, &content_type,
                                 &recv_content_size, &forward_flags]() {
    std::string header_name_lower = PMA_HELPER::ascii_str_to_lower(header_name);
    if (header_name_lower == "transfer-encoding" &&
        PMA_HELPER::ascii_str_to_lower(
            PMA_HELPER::trim_whitespace(header_value)) == "chunked") {
      forward_flags.set(0);
    }
    if (header_name_lower != "content-length" &&
        header_name_lower != "connection" &&
        header_name_lower != "accept-ranges") {
      // PMA_EPrintln("  recv header: {}: {}", header_name, header_value);
      content_type.append(std::format("{}: {}\r\n", header_name, header_value));
    } else {
      try {
        size_t content_size = std::stoull(header_value);
        if (content_size > 0) {
          recv_content_size = content_size;
          // PMA_EPrintln("DEBUG: content_size: {}", content_size);
        }
      } catch (const std::exception &e) {
      }
    }
    header_name.clear();
    header_value.clear();
  };

  while (wait_ticks < args.req_timeout_ticks) {
    skip_before_idx = 0;
    ssize_t read_ret = read(socket_fd, buf.data(), buf.size());
    if (read_ret == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        if (recv_content_size.has_value() && recv_content_size.value() == 0) {
          break;
        }
        std::this_thread::sleep_for(SLEEP_MILLISECONDS_CHRONO);
        ++wait_ticks;
        continue;
      } else {
        PMA_EPrintln("ERROR: Failed to read response, errno {}", errno);
        status = "HTTP/1.0 500 Internal Server Error";
        body =
            "<html><p>500 Internal Server Error</p><p>Failed to "
            "forward</p></html>";
        return;
      }
    } else if (read_ret == 0) {
      if (body.empty() || status.empty() || content_type.empty()) {
        PMA_EPrintln("ERROR: EOF, no response, errno {}", errno);
        status = "HTTP/1.0 500 Internal Server Error";
        body =
            "<html><p>500 Internal Server Error</p><p>Failed to "
            "forward</p></html>";
        return;
      }
      break;
    } else {
      wait_ticks = 0;
      const size_t read_size = static_cast<size_t>(read_ret);
      for (size_t idx = 0; idx < read_size; ++idx) {
        if (before_first_line) {
          if (buf.at(idx) != '\r') {
            status.push_back(buf.at(idx));
          } else if (read_size > idx + 1 && buf.at(idx + 1) == '\n') {
            before_first_line = 0;
            skip_before_idx = idx + 2;
          } else {
            PMA_EPrintln("ERROR: Invalid forwarded status line");
            status = "HTTP/1.0 500 Internal Server Error";
            body =
                "<html><p>500 Internal Server Error</p><p>Failed to "
                "forward</p></html>";
            return;
          }
        } else if (before_content) {
          if (idx < skip_before_idx) {
            continue;
          } else {
            skip_before_idx = 0;
          }

          if (buf.at(idx) != '\r') {
            if (buf.at(idx) == ':' && header_name.empty()) {
              header_name = std::move(temp);
              temp.clear();
            } else {
              temp.push_back(buf.at(idx));
            }
          } else if (read_size > idx + 3 && buf.at(idx + 1) == '\n' &&
                     buf.at(idx + 2) == '\r' && buf.at(idx + 3) == '\n') {
            if (!temp.empty() && !header_name.empty() && header_value.empty()) {
              header_value = PMA_HELPER::trim_whitespace(temp);
              temp.clear();
              verify_header_fn();
            } else if (!temp.empty()) {
              PMA_EPrintln(
                  "ERROR: Invalid forwarded last header internal state");
              status = "HTTP/1.0 500 Internal Server Error";
              content_type.clear();
              body =
                  "<html><p>500 Internal Server Error</p><p>Failed to "
                  "forward</p></html>";
              return;
            }
            before_content = 0;
            skip_before_idx = idx + 4;
          } else if (read_size > idx + 1 && buf.at(idx + 1) == '\n') {
            if (!temp.empty() && !header_name.empty() && header_value.empty()) {
              header_value = PMA_HELPER::trim_whitespace(temp);
              temp.clear();
              verify_header_fn();
              skip_before_idx = idx + 2;
            } else if (!temp.empty()) {
              PMA_EPrintln("ERROR: Invalid forwarded headers internal state");
              status = "HTTP/1.0 500 Internal Server Error";
              content_type.clear();
              body =
                  "<html><p>500 Internal Server Error</p><p>Failed to "
                  "forward</p></html>";
              return;
            }
          } else {
            PMA_EPrintln("ERROR: Invalid forwarded headers");
            status = "HTTP/1.0 500 Internal Server Error";
            content_type.clear();
            body =
                "<html><p>500 Internal Server Error</p><p>Failed to "
                "forward</p></html>";
            return;
          }
        } else if (idx < skip_before_idx) {
          continue;
        } else {
          // body.push_back(buf.at(idx));
          body.append(buf.data() + idx, read_size - idx);
          if (recv_content_size.has_value()) {
            if (recv_content_size.value() < read_size - idx) {
              PMA_EPrintln(
                  "ERROR: Invalid state handling content size: size is {}, "
                  "value is {}",
                  recv_content_size.value(), read_size - idx);
              status = "HTTP/1.0 500 Internal Server Error";
              content_type.clear();
              body =
                  "<html><p>500 Internal Server Error</p><p>Failed to "
                  "forward</p></html>";
              return;
            } else {
              recv_content_size.value() -= read_size - idx;
              // PMA_EPrintln("DEBUG: recv_content_size now {}",
              //              recv_content_size.value());
            }
          }
          break;
        }
      }  // for idx < read_size
    }
  }  // while

  if (status.empty()) {
    status = "HTTP/1.0 500 Internal Server Error";
    content_type = "Connection: close";
    body =
        "<html><p>500 Internal Server Error</p><p>Failed to "
        "forward, no response</p></html>";
  } else {
    // Append "Connection: close" without ending "\r\n" as it is added later.
    content_type.append("Connection: close");
  }
}

struct ThreadData {
  AddrPortInfo addr_port_info;
  const PMA_ARGS::Args *args;
  const std::optional<PMA_MSQL::Conf> *msql_conf_opt;
  std::mutex *cached_allowed_mutex;
  std::unordered_map<std::string,
                     std::chrono::time_point<std::chrono::steady_clock> >
      *cached_allowed;
  int conn_fd;
};

void thread_handle_connection_fn(void *ud) {
  ThreadData *data = reinterpret_cast<ThreadData *>(ud);
  std::array<char, REQ_READ_BUF_SIZE> buf;

  // Lazy load the connection to msql.
  std::optional<PMA_MSQL::Connection> msql_conn_opt;

  while (data->addr_port_info.ticks < THREAD_TIMEOUT_TICKS) {
    std::this_thread::sleep_for(SLEEP_MILLISECONDS_CHRONO);
    data->addr_port_info.ticks += 1;

    auto time_now = std::chrono::steady_clock::now();

    if (data->addr_port_info.remaining_buffer.has_value()) {
      ssize_t write_ret = write(
          data->conn_fd, data->addr_port_info.remaining_buffer.value().data(),
          data->addr_port_info.remaining_buffer.value().size());
      if (write_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // Nonblocking-IO indicating to write later, intentionally left
          // blank.
        } else {
          PMA_EPrintln(
              "ERROR: Failed to send partial response to client {} "
              "(write_ret {}, errno {})!",
              data->addr_port_info.client_addr, write_ret, errno);
          break;
        }
      } else if (write_ret !=
                 static_cast<ssize_t>(
                     data->addr_port_info.remaining_buffer.value().size())) {
        if (write_ret > 0) {
          data->addr_port_info.remaining_buffer =
              data->addr_port_info.remaining_buffer.value().substr(
                  static_cast<size_t>(write_ret));
        } else {
          PMA_EPrintln(
              "ERROR: Failed to send partial response to client {} "
              "(write_ret {}, errno {})!",
              data->addr_port_info.client_addr, write_ret, errno);
          break;
        }
      } else {
        data->addr_port_info.remaining_buffer = std::nullopt;
        data->addr_port_info.ticks = 0;
      }
      continue;
    }

    ssize_t read_ret = read(data->conn_fd, buf.data(), buf.size() - 1);
    if (read_ret == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Nonblocking-IO indicating no bytes to read
        continue;
      } else {
        PMA_Println("Failed to read from client {} (errno {})",
                    data->addr_port_info.client_addr, errno);
        break;
        continue;
      }
    }
    if (read_ret > 0) {
      data->addr_port_info.ticks = 0;
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
        if (data->args->flags.test(0)) {
          if (auto fiter = req.headers.find("x-real-ip");
              fiter != req.headers.end()) {
#ifndef NDEBUG
            PMA_Println("x-real-ip header found, changing client addr: {}",
                        fiter->second);
#endif
            data->addr_port_info.client_addr = fiter->second;
          }
        }

        std::string status = "HTTP/1.0 200 OK";
        std::string content_type = "Content-type: text/html; charset=utf-8";
        std::string body;
        PMA_SQL::SQLITECtx sqliteCtx;
        PMA_SQL::ErrorT err = PMA_SQL::ErrorT::SUCCESS;
        std::string msg;
        if (!data->args->flags.test(4)) {
          std::tie(sqliteCtx, err, msg) =
              PMA_SQL::init_sqlite(data->args->sqlite_path);
        }

        // 0 - content-type: chunked
        std::bitset<32> forward_flags;

        if (err != PMA_SQL::ErrorT::SUCCESS) {
          PMA_EPrintln("ERROR: Failed to initialize sqlite: {}, {}",
                       PMA_SQL::error_t_to_string(err), msg);
          status = "HTTP/1.0 500 Internal Server Error";
          body =
              "<html><p>500 Internal Server Error</p><p>Failed to init "
              "db</p></html>";
        } else if (req.url_or_err_msg == data->args->api_url) {
          const auto [err, json_keyvals] =
              PMA_HTTP::parse_simple_json(req.body);
          if (err != PMA_HTTP::ErrorT::SUCCESS) {
            PMA_EPrintln("ERROR: Failed to parse json from client {}!",
                         data->addr_port_info.client_addr);
            status = "HTTP/1.0 500 Internal Server Error";
            body =
                "<html><p>500 Internal Server Error</p><p>Failed to parse "
                "json</p></html>";
          } else if (json_keyvals.find("type") == json_keyvals.end() ||
                     json_keyvals.find("id") == json_keyvals.end() ||
                     json_keyvals.find("factors") == json_keyvals.end()) {
            PMA_EPrintln("ERROR: Client {} omitted necessary info!",
                         data->addr_port_info.client_addr);
            status = "HTTP/1.0 400 Bad Request";
            body = "<html><p>400 Bad Request</p><p>Missing info</p></html>";
          } else if (data->args->flags.test(4)) {
            bool ping_ok = false;
            if (!msql_conn_opt.has_value() || !msql_conn_opt->is_valid()) {
              msql_conn_opt = PMA_MSQL::Connection::connect_msql(
                  (*data->msql_conf_opt)->addr, (*data->msql_conf_opt)->port,
                  (*data->msql_conf_opt)->user, (*data->msql_conf_opt)->pass,
                  (*data->msql_conf_opt)->db);
              if (!msql_conn_opt.has_value() || !msql_conn_opt->ping_check()) {
                PMA_EPrintln(
                    "ERROR: Thread failed to connect with MSQL server!");
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
                  msql_conn_opt.value(), data->args->challenge_timeout,
                  json_keyvals.find("id")->second,
                  json_keyvals.find("factors")->second,
                  data->addr_port_info.client_addr);
              if (err == PMA_MSQL::Error::SUCCESS) {
                PMA_Println("Challenge success from {}:{} port {}",
                            data->addr_port_info.client_addr,
                            data->addr_port_info.remote_port,
                            data->addr_port_info.local_port);
                content_type = "Content-type: text/plain";
                body = "Correct";
                std::unique_lock<std::mutex> cached_allowed_lock(
                    *data->cached_allowed_mutex);
                data->cached_allowed->insert(std::make_pair(
                    std::format("{}:{}", data->addr_port_info.client_addr,
                                data->addr_port_info.local_port),
                    time_now));
              } else {
                PMA_EPrintln(
                    "Warning: Client {}:{} -> {} failed challenge due to {}",
                    data->addr_port_info.client_addr,
                    data->addr_port_info.remote_port,
                    data->addr_port_info.local_port,
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
                data->addr_port_info.client_addr,
                json_keyvals.find("id")->second);
            if (err != PMA_SQL::ErrorT::SUCCESS) {
              PMA_EPrintln(
                  "Warning: Client {}:{} -> {} failed challenge due to {}, "
                  "{}",
                  data->addr_port_info.client_addr,
                  data->addr_port_info.remote_port,
                  data->addr_port_info.local_port,
                  PMA_SQL::error_t_to_string(err), msg);
              status = "HTTP/1.0 400 Bad Request";
              content_type = "Content-type: text/plain";
              body = "Incorrect";
            } else {
              PMA_Println("Challenge success from {}:{} port {}",
                          data->addr_port_info.client_addr,
                          data->addr_port_info.remote_port,
                          data->addr_port_info.local_port);
              content_type = "Content-type: text/plain";
              body = "Correct";
              std::unique_lock<std::mutex> cached_allowed_lock(
                  *data->cached_allowed_mutex);
              data->cached_allowed->insert(std::make_pair(
                  std::format("{}:{}", data->addr_port_info.client_addr,
                              data->addr_port_info.local_port),
                  time_now));
            }
          }

        } else if (req.url_or_err_msg == data->args->js_factors_url) {
          if (auto id_iter = req.queries.find("id");
              id_iter != req.queries.end()) {
            if (data->args->flags.test(4)) {
              bool ping_ok = false;
              if (!msql_conn_opt.has_value() || !msql_conn_opt->is_valid()) {
                msql_conn_opt = PMA_MSQL::Connection::connect_msql(
                    (*data->msql_conf_opt)->addr, (*data->msql_conf_opt)->port,
                    (*data->msql_conf_opt)->user, (*data->msql_conf_opt)->pass,
                    (*data->msql_conf_opt)->db);
                if (!msql_conn_opt.has_value() ||
                    !msql_conn_opt->ping_check()) {
                  PMA_EPrintln(
                      "ERROR: Thread failed to connect with MSQL server!");
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
                          msql_conn_opt.value(),
                          data->addr_port_info.client_addr, port,
                          data->args->factors, data->args->challenge_timeout);
                  if (cf_err == PMA_MSQL::Error::SUCCESS) {
                    PMA_Println("Requested challenge from {}:{} -> {}",
                                data->addr_port_info.client_addr,
                                data->addr_port_info.remote_port,
                                data->addr_port_info.local_port);
                    body = JS_FACTORS_WORKER;
                    PMA_HELPER::str_replace_all(body, "{API_URL}",
                                                data->args->api_url);
                    PMA_HELPER::str_replace_all(body, "{LARGE_NUMBER}", chall);
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
                        "WARNING: Bad request from client {}:{} -> {} due to "
                        "{}",
                        data->addr_port_info.client_addr,
                        data->addr_port_info.remote_port,
                        data->addr_port_info.local_port,
                        PMA_MSQL::error_to_str(itp_err));
                    status = "HTTP/1.0 400 Bad Request";
                    body = "<html><p>400 Bad Request</p><p>(No id)</p></html>";
                  } else {
                    PMA_EPrintln(
                        "WARNING: handling client {}:{} -> {} due to {}",
                        data->addr_port_info.client_addr,
                        data->addr_port_info.remote_port,
                        data->addr_port_info.local_port,
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
                                                data->args->challenge_timeout);
              const auto [err, msg_or_chal, answ, id] =
                  PMA_SQL::generate_challenge(sqliteCtx, data->args->factors,
                                              data->addr_port_info.client_addr,
                                              id_iter->second);
              if (err != PMA_SQL::ErrorT::SUCCESS) {
                PMA_EPrintln(
                    "ERROR: Failed to prepare challenge for client {}: {}, "
                    "{}",
                    data->addr_port_info.client_addr,
                    PMA_SQL::error_t_to_string(err), msg_or_chal);
                status = "HTTP/1.0 500 Internal Server Error";
                body =
                    "<html><p>500 Internal Server Error</p><p>Failed to "
                    "prepare challenge</p></html>";
              } else {
                PMA_Println("Requested challenge from {}:{} -> {}",
                            data->addr_port_info.client_addr,
                            data->addr_port_info.remote_port,
                            data->addr_port_info.local_port);
                body = JS_FACTORS_WORKER;
                PMA_HELPER::str_replace_all(body, "{API_URL}",
                                            data->args->api_url);
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
        } else if (data->args->flags.test(4)) {
          {
            std::unique_lock<std::mutex> cached_allowed_lock(
                *data->cached_allowed_mutex);
            if (auto cached_iter = data->cached_allowed->find(
                    std::format("{}:{}", data->addr_port_info.client_addr,
                                data->addr_port_info.local_port));
                cached_iter != data->cached_allowed->end()) {
              if (time_now - cached_iter->second > CACHED_TIMEOUT_T) {
                data->cached_allowed->erase(cached_iter);
              } else {
                cached_allowed_lock.unlock();
                if (data->args->flags.test(5)) {
                  do_curl_forwarding(data->addr_port_info.client_addr,
                                     data->addr_port_info.local_port, body,
                                     status, content_type, req, *data->args);
                } else {
                  do_ipv4_socket_forwarding(data->addr_port_info.client_addr,
                                            data->addr_port_info.local_port,
                                            body, status, content_type,
                                            forward_flags, req, *data->args);
                }
                goto PMA_RESPONSE_SEND_LOCATION;
              }
            }
          }

          bool ping_ok = false;
          if (!msql_conn_opt.has_value() || !msql_conn_opt->is_valid()) {
            msql_conn_opt = PMA_MSQL::Connection::connect_msql(
                (*data->msql_conf_opt)->addr, (*data->msql_conf_opt)->port,
                (*data->msql_conf_opt)->user, (*data->msql_conf_opt)->pass,
                (*data->msql_conf_opt)->db);
            if (!msql_conn_opt.has_value() || !msql_conn_opt->ping_check()) {
              PMA_EPrintln("ERROR: Thread failed to connect with MSQL server!");
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
                msql_conn_opt.value(), data->addr_port_info.client_addr,
                data->addr_port_info.local_port, data->args->allowed_timeout);
            if (is_allowed_e == PMA_MSQL::Error::SUCCESS) {
              std::unique_lock<std::mutex> cached_allowed_lock(
                  *data->cached_allowed_mutex);
              data->cached_allowed->insert(std::make_pair(
                  std::format("{}:{}", data->addr_port_info.client_addr,
                              data->addr_port_info.local_port),
                  time_now));
              cached_allowed_lock.unlock();
              if (data->args->flags.test(5)) {
                do_curl_forwarding(data->addr_port_info.client_addr,
                                   data->addr_port_info.local_port, body,
                                   status, content_type, req, *data->args);
              } else {
                do_ipv4_socket_forwarding(data->addr_port_info.client_addr,
                                          data->addr_port_info.local_port, body,
                                          status, content_type, forward_flags,
                                          req, *data->args);
              }
              goto PMA_RESPONSE_SEND_LOCATION;
            } else if (is_allowed_e == PMA_MSQL::Error::EMPTY_QUERY_RESULT) {
              const auto [err, id] = PMA_MSQL::init_id_to_port(
                  msql_conn_opt.value(), data->addr_port_info.local_port,
                  data->args->challenge_timeout);
              if (err == PMA_MSQL::Error::SUCCESS) {
                body = HTML_BODY_FACTORS;
                PMA_HELPER::str_replace_all(
                    body, "{JS_FACTORS_URL}",
                    std::format("{}?id={}", data->args->js_factors_url, id));
              } else {
                PMA_EPrintln(
                    "ERROR: Failed to init id-to-port for client {}! {}",
                    data->addr_port_info.client_addr,
                    PMA_MSQL::error_to_str(err));
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
              PMA_EPrintln("ERROR: Failed to check if client {} is allowed: {}",
                           data->addr_port_info.client_addr,
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
          {
            std::unique_lock<std::mutex> cached_allowed_lock(
                *data->cached_allowed_mutex);
            if (auto cached_iter = data->cached_allowed->find(
                    std::format("{}:{}", data->addr_port_info.client_addr,
                                data->addr_port_info.local_port));
                cached_iter != data->cached_allowed->end()) {
              if (time_now - cached_iter->second > CACHED_TIMEOUT_T) {
                data->cached_allowed->erase(cached_iter);
              } else {
                cached_allowed_lock.unlock();
                if (data->args->flags.test(5)) {
                  do_curl_forwarding(data->addr_port_info.client_addr,
                                     data->addr_port_info.local_port, body,
                                     status, content_type, req, *data->args);
                } else {
                  do_ipv4_socket_forwarding(data->addr_port_info.client_addr,
                                            data->addr_port_info.local_port,
                                            body, status, content_type,
                                            forward_flags, req, *data->args);
                }
                goto PMA_RESPONSE_SEND_LOCATION;
              }
            }
          }

          PMA_SQL::cleanup_stale_entries(sqliteCtx,
                                         data->args->allowed_timeout);

          const auto [err, msg, is_allowed] = PMA_SQL::is_allowed_ip_port(
              sqliteCtx, data->addr_port_info.client_addr,
              data->addr_port_info.local_port);
          if (err != PMA_SQL::ErrorT::SUCCESS || !is_allowed) {
            PMA_SQL::cleanup_stale_id_to_ports(sqliteCtx,
                                               data->args->challenge_timeout);
            const auto [err, msg, id] = PMA_SQL::init_id_to_port(
                sqliteCtx, data->addr_port_info.local_port);
            body = HTML_BODY_FACTORS;
            PMA_HELPER::str_replace_all(
                body, "{JS_FACTORS_URL}",
                std::format("{}?id={}", data->args->js_factors_url, id));
          } else {
            std::unique_lock<std::mutex> cached_allowed_lock(
                *data->cached_allowed_mutex);
            data->cached_allowed->insert(std::make_pair(
                std::format("{}:{}", data->addr_port_info.client_addr,
                            data->addr_port_info.local_port),
                time_now));
            cached_allowed_lock.unlock();
            if (data->args->flags.test(5)) {
              do_curl_forwarding(data->addr_port_info.client_addr,
                                 data->addr_port_info.local_port, body, status,
                                 content_type, req, *data->args);
            } else {
              do_ipv4_socket_forwarding(data->addr_port_info.client_addr,
                                        data->addr_port_info.local_port, body,
                                        status, content_type, forward_flags,
                                        req, *data->args);
            }
            goto PMA_RESPONSE_SEND_LOCATION;
          }
        }

      PMA_RESPONSE_SEND_LOCATION:
        std::string full;
        if (forward_flags.test(0)) {
          full = std::format("{}\r\n{}\r\n\r\n{}", status, content_type, body);
        } else {
          full = std::format("{}\r\n{}\r\nContent-Length: {}\r\n\r\n{}", status,
                             content_type, body.size(), body);
        }
        ssize_t write_ret = write(data->conn_fd, full.data(), full.size());
        if (write_ret != static_cast<ssize_t>(full.size())) {
          if (write_ret > 0) {
            data->addr_port_info.remaining_buffer =
                full.substr(static_cast<size_t>(write_ret));
          } else {
            PMA_EPrintln(
                "ERROR: Failed to send response to client {} (write_ret {})!",
                data->addr_port_info.client_addr, write_ret);
            break;
          }
        } else if (write_ret == -1) {
          PMA_EPrintln("ERROR: Failed to write to client {}, errno {}!",
                       data->addr_port_info.client_addr, errno);
          break;
        } else {
          // Success, intentionally left blank.
        }
      } else {
        PMA_EPrintln("ERROR {}: {}", PMA_HTTP::error_t_to_str(req.error_enum),
                     req.url_or_err_msg);
        break;
      }
    } else if (read_ret == 0) {
#ifndef NDEBUG
      PMA_Println("EOF From client {}:{} -> {}, closing...",
                  data->addr_port_info.client_addr,
                  data->addr_port_info.remote_port,
                  data->addr_port_info.local_port);
#endif
      break;
    }
  }  // while ticks < timeout ticks

  if (data->addr_port_info.ticks >= THREAD_TIMEOUT_TICKS) {
#ifndef NDEBUG
    PMA_Println("Timed out connection from {}:{} on port {}",
                data->addr_port_info.client_addr,
                data->addr_port_info.remote_port,
                data->addr_port_info.local_port);
#endif
  }
}

void thread_cleanup_fn(void *ud) {
  ThreadData *data = reinterpret_cast<ThreadData *>(ud);
  if (data->conn_fd > 0) {
    close(data->conn_fd);
  }
  delete data;
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

  PMA_HELPER::MimeTypes mime_types{};

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

  ThreadPool thread_pool;
  if (args.thread_count <= 1) {
    thread_pool.set_thread_count(1);
  } else {
    thread_pool.set_thread_count(args.thread_count);
  }

  PMA_HELPER::set_signal_handler(SIGINT, receive_signal);
  PMA_HELPER::set_signal_handler(SIGHUP, receive_signal);
  PMA_HELPER::set_signal_handler(SIGTERM, receive_signal);

  struct sockaddr_in sain4;
  std::memset(&sain4, 0, sizeof(struct sockaddr_in));
  struct sockaddr_in6 sain6;
  std::memset(&sain6, 0, sizeof(struct sockaddr_in6));
  socklen_t sain_len;

  // std::deque<int> to_remove_connections;
  // std::array<char, REQ_READ_BUF_SIZE> buf;

  std::mutex cached_allowed_mutex;
  std::unordered_map<std::string,
                     std::chrono::time_point<std::chrono::steady_clock> >
      cached_allowed;
  std::chrono::time_point<std::chrono::steady_clock> time_now =
      std::chrono::steady_clock::now();
  std::chrono::time_point<std::chrono::steady_clock> time_prev = time_now;

  int ret;
  while (!interrupt_received) {
    std::this_thread::sleep_for(SLEEP_MILLISECONDS_CHRONO);
    time_now = std::chrono::steady_clock::now();
    if (time_now - time_prev > CACHED_CLEAR_T) {
      time_prev = time_now;
      std::unique_lock<std::mutex> cached_allowed_lock(cached_allowed_mutex);
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
                      iter->second.local_port);
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

          ThreadData *new_data = new ThreadData;
          new_data->addr_port_info = iter->second;
          new_data->addr_port_info.client_addr = std::move(client_ipv4);
          new_data->addr_port_info.remote_port =
              PMA_HELPER::be_swap_u16(sain4.sin_port);
          new_data->args = &args;
          new_data->msql_conf_opt = &msql_conf_opt;
          new_data->cached_allowed_mutex = &cached_allowed_mutex;
          new_data->cached_allowed = &cached_allowed;
          new_data->conn_fd = ret;

          thread_pool.add_func(thread_handle_connection_fn, new_data,
                               thread_cleanup_fn);
        } else {
          // IPV6 new connection
          std::string client_ipv6 = PMA_HTTP::ipv6_addr_to_str(
              *reinterpret_cast<std::array<uint8_t, 16> *>(
                  sain6.sin6_addr.s6_addr));
#ifndef NDEBUG
          PMA_Println("New connection from {}:{} on port {}", client_ipv6,
                      PMA_HELPER::be_swap_u16(sain6.sin6_port),
                      iter->second.local_port);
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

          ThreadData *new_data = new ThreadData;
          new_data->addr_port_info = iter->second;
          new_data->addr_port_info.client_addr = std::move(client_ipv6);
          new_data->addr_port_info.remote_port =
              PMA_HELPER::be_swap_u16(sain4.sin_port);
          new_data->args = &args;
          new_data->msql_conf_opt = &msql_conf_opt;
          new_data->cached_allowed_mutex = &cached_allowed_mutex;
          new_data->cached_allowed = &cached_allowed;
          new_data->conn_fd = ret;

          thread_pool.add_func(thread_handle_connection_fn, new_data,
                               thread_cleanup_fn);
        }
      }  // while (ret >= 0)
    }  // for (sockets ... )

    std::cout << std::flush;
  }

  return 0;
}

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

// Local includes.
#include "args.h"
#include "helpers.h"
#include "http.h"
#include "poor_mans_print.h"

constexpr unsigned int SLEEP_MILLISECONDS = 10;
// 5 seconds
constexpr unsigned int TIMEOUT_ITER_TICKS = 5000 / SLEEP_MILLISECONDS;

volatile int interrupt_received = 0;

void receive_signal(int sig) {
  PMA_Println("Interupt received...");
  // SIGHUP, SIGINT, SIGTERM
  // if (sig == 1 || sig == 2 || sig == 15) {
  //}
  interrupt_received = 1;
}

// First string is host addr, second string is client addr (to be filled in)
// First uint16_t is port, second uint16_t is iteration tick count
// Bitset flags:
// 0 - is ipv4
using AddrPortInfo =
    std::tuple<std::string, std::string, std::bitset<16>, uint16_t, uint16_t>;

AddrPortInfo conv_addr_port(const PMA_ARGS::AddrPort &addr_port, bool is_ipv4) {
  AddrPortInfo tuple =
      std::make_tuple(std::get<0>(addr_port), std::string{}, std::bitset<16>{},
                      std::get<1>(addr_port), 0);

  std::get<std::bitset<16> >(tuple).set(0, is_ipv4);

  return tuple;
}

int main(int argc, char **argv) {
  const PMA_ARGS::Args args(argc, argv);

  if (args.flags.test(2)) {
    PMA_EPrintln("ERROR: Failed to parse args!");
    return 3;
  }

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
    const auto [err, msg, socket_fd] =
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
            "\"{}\"!",
            std::get<0>(a), std::get<1>(a));
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

  signal(SIGINT, receive_signal);
  signal(SIGHUP, receive_signal);
  signal(SIGTERM, receive_signal);

  struct sockaddr_in sain4;
  std::memset(&sain4, 0, sizeof(struct sockaddr_in));
  struct sockaddr_in6 sain6;
  std::memset(&sain6, 0, sizeof(struct sockaddr_in6));
  socklen_t sain_len;

  std::deque<int> to_remove_connections;

  int ret;
  const auto sleep_duration = std::chrono::milliseconds(SLEEP_MILLISECONDS);
  while (!interrupt_received) {
    std::this_thread::sleep_for(sleep_duration);

    // Fetch new connections
    for (auto iter = sockets.begin(); iter != sockets.end(); ++iter) {
      if (std::get<std::bitset<16> >(iter->second).test(0)) {
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
                       sain_len, sizeof(struct sockaddr_in));
        }
      }

      if (ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // Nonblocking-IO indicating no connection to accept
          continue;
        } else {
          PMA_EPrintln(
              "WARNING: Failed to accept connection from socket {} (errno "
              "{})",
              iter->first, errno);
          continue;
        }
      } else if (std::get<std::bitset<16> >(iter->second).test(0)) {
        // IPV4 new connection
        std::string client_ipv4 =
            PMA_HTTP::ipv4_addr_to_str(sain4.sin_addr.s_addr);
        PMA_Println("New connection from {}:{} on port {}", client_ipv4,
                    PMA_HELPER::be_swap_u16(sain4.sin_port),
                    std::get<3>(iter->second));

        // Set nonblocking-IO on received connection fd
        int fcntl_ret = fcntl(ret, F_SETFL, O_NONBLOCK);
        if (fcntl_ret < 0) {
          PMA_EPrintln(
              "WARNING: Failed to set NONBLOCK on new connection (errno {}), "
              "closing connection...",
              errno);
          close(ret);
          continue;
        }

        std::get<1>(iter->second) = std::move(client_ipv4);
        connections.emplace(ret, iter->second);
      } else {
        // IPV6 new connection
        std::string client_ipv6 = PMA_HTTP::ipv6_addr_to_str(
            *reinterpret_cast<std::array<uint8_t, 16> *>(
                sain6.sin6_addr.s6_addr));
        PMA_Println("New connection from {}:{} on port {}", client_ipv6,
                    PMA_HELPER::be_swap_u16(sain6.sin6_port),
                    std::get<3>(iter->second));

        // Set nonblocking-IO on received connection fd
        int fcntl_ret = fcntl(ret, F_SETFL, O_NONBLOCK);
        if (fcntl_ret < 0) {
          PMA_EPrintln(
              "WARNING: Failed to set NONBLOCK on new connection (errno {}), "
              "closing connection...",
              errno);
          close(ret);
          continue;
        }

        std::get<1>(iter->second) = std::move(client_ipv6);
        connections.emplace(ret, iter->second);
      }
    }

    // Handle connections
    for (auto iter = connections.begin(); iter != connections.end(); ++iter) {
      std::get<4>(iter->second) += 1;
      if (std::get<4>(iter->second) >= TIMEOUT_ITER_TICKS) {
        PMA_Println("Timed out connection from {} on port {}",
                    std::get<1>(iter->second), std::get<3>(iter->second));
        to_remove_connections.push_back(iter->first);
        continue;
      }

      char buf[4096];
      ssize_t read_ret = read(iter->first, buf, 4095);
      if (read_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // Nonblocking-IO indicating no bytes to read
          continue;
        } else {
          PMA_Println("Failed to read from client {} (errno {})",
                      std::get<1>(iter->second), errno);
          to_remove_connections.push_back(iter->first);
          continue;
        }
      }
      if (read_ret > 0) {
        buf[read_ret] = 0;
        const auto [err, msg_or_url, q_params] =
            PMA_HTTP::handle_request_parse(buf);
        if (err == PMA_HTTP::ErrorT::SUCCESS) {
          PMA_Println("URL: {}, Params:", msg_or_url);
          for (auto qiter = q_params.begin(); qiter != q_params.end();
               ++qiter) {
            PMA_Println("  {}={}", qiter->first, qiter->second);
          }
          std::string body = "<html>Test</html>\n";
          std::string full = std::format(
              "HTTP/1.0 200 OK\r\nContent-type: text/html; "
              "charset=utf-8\r\nContent-Length: {}\r\n\r\n{}",
              body.size(), body);
          ssize_t write_ret = write(iter->first, full.c_str(), full.size());
          if (write_ret != static_cast<ssize_t>(full.size())) {
            PMA_EPrintln(
                "ERROR: Failed to send response to client {} (write_ret {})!",
                std::get<1>(iter->second), write_ret);
            to_remove_connections.push_back(iter->first);
          }
        } else {
          PMA_EPrintln("ERROR {}: {}", PMA_HTTP::error_t_to_str(err),
                       msg_or_url);
          to_remove_connections.push_back(iter->first);
        }
      }
      // TODO: Receive/Send data from/to connections
    }

    // Remove connections
    for (int connection_fd : to_remove_connections) {
      close(connection_fd);
      connections.erase(connection_fd);
    }
    to_remove_connections.clear();
  }

  return 0;
}

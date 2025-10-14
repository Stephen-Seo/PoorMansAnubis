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
#include <chrono>
#include <optional>
#include <thread>

// Unix includes
#include <signal.h>
#include <unistd.h>

// Local includes.
#include "args.h"
#include "helpers.h"
#include "http.h"
#include "poor_mans_print.h"

constexpr unsigned int SLEEP_MILLISECONDS = 10;

volatile int interrupt_received = 0;

void receive_signal(int sig) {
  PMA_Println("Interupt received...");
  // SIGHUP, SIGINT, SIGTERM
  // if (sig == 1 || sig == 2 || sig == 15) {
  //}
  interrupt_received = 1;
}

int main(int argc, char **argv) {
  const PMA_ARGS::Args args(argc, argv);

  if (args.flags.test(2)) {
    PMA_EPrintln("ERROR: Failed to parse args!");
    return 3;
  }

  std::deque<int> sockets;
  GenericCleanup<std::deque<int> *> cleanup_sockets(
      &sockets, [](std::deque<int> **s) {
        PMA_Println("Cleaning up sockets...");
        for (int &fd : **s) {
          if (fd >= 0) {
            close(fd);
            fd = -1;
          }
        }
      });

  for (const PMA_ARGS::AddrPort &a : args.addr_ports) {
    std::optional<int> socket_fd_opt;
    const auto [err, msg, socket_fd] =
        PMA_HTTP::get_ipv6_socket_server(std::get<0>(a), std::get<1>(a));
    if (err == PMA_HTTP::ErrorT::SUCCESS) {
      socket_fd_opt = socket_fd;
    } else {
      const auto [err, msg, socket_fd] =
          PMA_HTTP::get_ipv4_socket_server(std::get<0>(a), std::get<1>(a));
      if (err == PMA_HTTP::ErrorT::SUCCESS) {
        socket_fd_opt = socket_fd;
      } else {
        PMA_EPrintln(
            "ERROR: Failed to get listening socket for addr \"{}\" on port "
            "\"{}\"!",
            std::get<0>(a), std::get<1>(a));
        return 1;
      }
    }

    if (socket_fd_opt.has_value() && socket_fd_opt.value() >= 0) {
      sockets.push_back(socket_fd_opt.value());
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

  std::deque<int> connections;
  GenericCleanup<std::deque<int> *> cleanup_connections(
      &connections, [](std::deque<int> **s) {
        PMA_Println("Cleaning up connections...");
        for (int &fd : **s) {
          if (fd >= 0) {
            close(fd);
            fd = -1;
          }
        }
      });

  signal(SIGINT, receive_signal);
  signal(SIGHUP, receive_signal);
  signal(SIGTERM, receive_signal);

  const auto sleep_duration = std::chrono::milliseconds(SLEEP_MILLISECONDS);
  while (!interrupt_received) {
    std::this_thread::sleep_for(sleep_duration);
  }

  return 0;
}

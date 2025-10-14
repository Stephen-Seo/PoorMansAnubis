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

#include "args.h"

// Standard library includes
#include <climits>
#include <cstdlib>
#include <cstring>

// local includes
#include "poor_mans_print.h"

PMA_ARGS::Args::Args(int argc, char **argv)
    : factors(DEFAULT_FACTORS_DIGITS),
      default_dest_url("https://seodisparate.com"),
      addr_ports(),
      port_to_dest_urls(),
      flags(),
      api_url("/pma_api"),
      js_factors_url("/pma_factors.js"),
      challenge_timeout(CHALLENGE_FACTORS_TIMEOUT_MINUTES),
      allowed_timeout(ALLOWED_IP_TIMEOUT_MINUTES) {
  --argc;
  ++argv;

  while (argc > 0) {
    if (std::strncmp(argv[0], "--factors=", 10) == 0) {
      this->factors = std::strtoull(argv[0] + 10, nullptr, 10);
      if (factors == 0 || factors == ULLONG_MAX) {
        PMA_EPrintln(
            "ERROR: Failed to parse args! Invalid --factors=<digits>!");
        flags.set(2);
        return;
      }
    } else if (std::strncmp(argv[0], "--dest-url=", 11) == 0) {
      this->default_dest_url = std::string(argv[0] + 11);
      if (this->default_dest_url.empty()) {
        PMA_EPrintln("ERROR: Got empty --dest-url=<url>!");
        flags.set(2);
        return;
      }
    } else if (std::strncmp(argv[0], "--addr-port=", 12) == 0) {
      std::string addr;
      std::string port_temp;
      uint16_t port;

      bool first = true;
      for (size_t idx = 12; argv[0][idx] != 0; ++idx) {
        if (first) {
          if (argv[0][idx] == ':') {
            first = false;
            continue;
          } else {
            addr.push_back(argv[0][idx]);
          }
        } else {
          port_temp.push_back(argv[0][idx]);
        }
      }

      if (first || port_temp.empty() || addr.empty()) {
        PMA_EPrintln("ERROR: Failed to parse --addr-port=<addr>:<port> !");
        flags.set(2);
        return;
      }

      try {
        unsigned long parsed = std::stoul(port_temp);
        if (parsed > 0xFFFF) {
          PMA_EPrintln(
              "ERROR: Failed to parse port from "
              "--addr-port=<addr>:<port> (port number too large)!");
          flags.set(2);
          return;
        }
        port = static_cast<uint16_t>(parsed);
      } catch (const std::invalid_argument &e) {
        PMA_EPrintln(
            "ERROR: Failed to parse port from "
            "--addr-port=<addr>:<port> (invalid argument)!");
        flags.set(2);
        return;
      } catch (const std::out_of_range &e) {
        PMA_EPrintln(
            "ERROR: Failed to parse port from "
            "--addr-port=<addr>:<port> (out of range)!");
        flags.set(2);
        return;
      }

      this->addr_ports.emplace_back(addr, port);
    } else if (std::strncmp(argv[0], "--port-to-dest-url=", 19) == 0) {
      std::string port_temp;
      std::string url;
      uint16_t port;

      bool first = true;
      for (size_t idx = 19; argv[0][idx] != 0; ++idx) {
        if (first) {
          if (argv[0][idx] == ':') {
            first = false;
            continue;
          } else {
            port_temp.push_back(argv[0][idx]);
          }
        } else {
          url.push_back(argv[0][idx]);
        }
      }

      if (first || url.empty() || port_temp.empty()) {
        PMA_EPrintln(
            "ERROR: Failed to parse --port-to-dest-url=<port>:<url> !");
        flags.set(2);
        return;
      }

      try {
        unsigned long parsed = std::stoul(port_temp);
        if (parsed > 0xFFFF) {
          PMA_EPrintln(
              "ERROR: Failed to parse port from "
              "--port-to-dest-url=<port>:<url> (port number too large)!");
          flags.set(2);
          return;
        }
        port = static_cast<uint16_t>(parsed);
      } catch (const std::invalid_argument &e) {
        PMA_EPrintln(
            "ERROR: Failed to parse port from "
            "--port-to-dest-url=<port>:<url> (invalid argument)!");
        flags.set(2);
        return;
      } catch (const std::out_of_range &e) {
        PMA_EPrintln(
            "ERROR: Failed to parse port from "
            "--port-to-dest-url=<port>:<url> (out of range)!");
        flags.set(2);
        return;
      }

      this->port_to_dest_urls.insert(std::make_pair(port, url));
    } else if (std::strcmp(argv[0], "--enable-x-real-ip-header") == 0) {
      flags.set(0);
    } else if (std::strncmp(argv[0], "--api-url=", 10) == 0) {
      this->api_url = std::string(argv[0] + 10);
      if (this->api_url.empty()) {
        PMA_EPrintln("ERROR: Failed to parse --api-url=<url> (url is empty)!");
        flags.set(2);
        return;
      }
    } else if (std::strncmp(argv[0], "--js-factors-url=", 17) == 0) {
      this->js_factors_url = std::string(argv[0] + 17);
      if (this->js_factors_url.empty()) {
        PMA_EPrintln(
            "ERROR: Failed to parse --js-factors-url=<url> (url is empty)!");
        flags.set(2);
        return;
      }
    } else if (std::strncmp(argv[0], "--challenge-timeout=", 20) == 0) {
      try {
        unsigned long parsed = std::stoul(std::string(argv[0] + 20));
        if (parsed > 0xFFFFFFFF) {
          PMA_EPrintln(
              "ERROR: Failed to parse timeout from "
              "--challenge-timeout=<minutes> (timeout too large)!");
          flags.set(2);
          return;
        }
        this->challenge_timeout = static_cast<uint32_t>(parsed);
      } catch (const std::invalid_argument &e) {
        PMA_EPrintln(
            "ERROR: Failed to parse timeout from "
            "--challenge-timeout=<minutes> (invalid argument)!");
        flags.set(2);
        return;
      } catch (const std::out_of_range &e) {
        PMA_EPrintln(
            "ERROR: Failed to parse timeout from "
            "--challenge-timeout=<minutes> (out of range)!");
        flags.set(2);
        return;
      }
    } else if (std::strncmp(argv[0], "--allowed-timeout=", 18) == 0) {
      try {
        unsigned long parsed = std::stoul(std::string(argv[0] + 18));
        if (parsed > 0xFFFFFFFF) {
          PMA_EPrintln(
              "ERROR: Failed to parse timeout from "
              "--allowed-timeout=<minutes> (timeout too large)!");
          flags.set(2);
          return;
        }
        this->allowed_timeout = static_cast<uint32_t>(parsed);
      } catch (const std::invalid_argument &e) {
        PMA_EPrintln(
            "ERROR: Failed to parse timeout from "
            "--allowed-timeout=<minutes> (invalid argument)!");
        flags.set(2);
        return;
      } catch (const std::out_of_range &e) {
        PMA_EPrintln(
            "ERROR: Failed to parse timeout from "
            "--allowed-timeout=<minutes> (out of range)!");
        flags.set(2);
        return;
      }
    } else if (std::strcmp(argv[0], "--enable-override-dest-url") == 0) {
      if (flags.test(3)) {
        flags.set(1);
      } else {
        PMA_EPrintln(
            "ERROR: You must first use \"--important-warning-has-hbeen-read\" "
            "option to enable this option! Please read the documentation to "
            "understand the security implications of this option! It may be "
            "better to just use mulitple \"--addr-port=...\" and "
            "\"--port-to-dest-url=...\" to accomplish the same thing!");
        flags.set(2);
        return;
      }
    } else if (std::strcmp(argv[0], "--important-warning-has-been-read") == 0) {
      flags.set(3);
    } else {
      PMA_EPrintln("ERROR Invalid argument: {}", argv[0]);
      flags.set(2);
      return;
    }

    --argc;
    ++argv;
  }
}

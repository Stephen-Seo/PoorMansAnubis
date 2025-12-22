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
#include "constants.h"
#include "poor_mans_print.h"

void pma_print_usage() {
  PMA_Println("Args:");
  PMA_Println(
      "  --factors=<digits> : Generate factors challenge with <digits> digits");
  PMA_Println("  --dest-url=<url> : Destination URL for verified clients;");
  PMA_Println("    example: \"--dest-url=http://127.0.0.1:9999\"");
  PMA_Println("  --addr-port=<addr>:<port> : Listening addr/port;");
  PMA_Println("    example: \"--addr-port=127.0.0.1:8080\"");
  PMA_Println(
      "  NOTICE: Specify --addr-port=... multiple times to listen on multiple "
      "ports");
  PMA_Println(
      "  NOTE: There is no longer a hard limit on the number of ports one can "
      "listen to");
  PMA_Println(
      "  --port-to-dest-url=<port>:<url> : Ensure requests from listening on "
      "<port> is forwarded to <url>");
  PMA_Println("  example: \"--port-to-dest-url=9001:https://example.com\"");
  PMA_Println(
      "  NOTICE: Specify --port-to-dest-url=... multiple times to add more "
      "mappings");
  PMA_Println(
      "  --mysql-conf=<config_file> : Set path to config file for mysql "
      "settings");
  PMA_Println("  --sqlite-path=<path> : Set filename for sqlite db");
  PMA_Println(
      "  --enable-x-real-ip-header : Enable trusting \"x-real-ip\" header as "
      "client ip addr");
  PMA_Println(
      "  --api-url=<url> : Set endpoint for client to POST to this software;");
  PMA_Println("    example: \"--api-url=/pma_api\"");
  PMA_Println(
      "  --js-factors-url=<url> : Set endpoint for client to request "
      "factors.js from this software;");
  PMA_Println("    example: \"--js-factors-url=/pma_factors.js\"");
  PMA_Println(
      "  --challenge-timeout=<minutes> : Set minutes for how long challenge "
      "answers are stored in db");
  PMA_Println(
      "  --allowed-timeout=<minutes> : Set how long a client is allowed to "
      "access before requiring challenge again");
  PMA_Println(
      "  --enable-override-dest-url : Enable \"override-dest-url\" request "
      "header to determine where to forward;");
  PMA_Println(
      "    example header: \"override-dest-url: http://127.0.0.1:8888\"");
  PMA_Println(
      "  WARNING: If --enable-override-dest-url is used, you must ensure that");
  PMA_Println(
      "    PoorMansAnubis always receives this header as set by your server. "
      "If you");
  PMA_Println(
      "    don't then anyone accessing your server may be able to set this "
      "header and");
  PMA_Println("    direct PoorMansAnubis to load any website!");
  PMA_Println(
      "    If you are going to use this anyway, you must ensure that a proper "
      "firewall is configured!");
  PMA_Println(
      "  --important-warning-has-been-read : Use this option to enable "
      "potentially dangerous options");
}

PMA_ARGS::Args::Args(int argc, char **argv)
    : factors(DEFAULT_FACTORS_DIGITS),
      default_dest_url("https://seodisparate.com"),
      addr_ports(),
      port_to_dest_urls(),
      flags(),
      api_url("/pma_api/"),
      js_factors_url("/pma_factors.js"),
      sqlite_path("sqlite_db"),
      challenge_timeout(CHALLENGE_FACTORS_TIMEOUT_MINUTES),
      allowed_timeout(ALLOWED_IP_TIMEOUT_MINUTES) {
  --argc;
  ++argv;

  if (argc == 0) {
    pma_print_usage();
    flags.set(2);
    return;
  }

  while (argc > 0) {
    if (std::strcmp(argv[0], "-h") == 0 ||
        std::strcmp(argv[0], "--help") == 0) {
      pma_print_usage();
      flags.set(2);
      return;
    } else if (std::strncmp(argv[0], "--factors=", 10) == 0) {
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
      } else if (!this->default_dest_url.starts_with("http")) {
        PMA_EPrintln("ERROR: --dest-url=<url> must start with \"http\"!");
        flags.set(2);
        return;
      }
    } else if (std::strncmp(argv[0], "--addr-port=", 12) == 0) {
      std::string addr;
      std::string port_temp;
      uint16_t port;

      // Find last colon as colons are used in ipv6
      size_t last_colon_idx = 0;
      for (size_t idx = 12; argv[0][idx] != 0; ++idx) {
        if (argv[0][idx] == ':') {
          last_colon_idx = idx;
        }
      }
      if (last_colon_idx == 0) {
        PMA_EPrintln("ERROR: Invalid address for --addr-port=... !");
        flags.set(2);
        return;
      }

      bool first = true;
      for (size_t idx = 12; argv[0][idx] != 0; ++idx) {
        if (first) {
          if (idx == last_colon_idx) {
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
      } else if (!url.starts_with("http")) {
        PMA_EPrintln(
            "ERROR: --port-to-dest-url=<port>:<url>, url must start with "
            "\"http\"!");
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
    } else if (std::strncmp(argv[0], "--mysql-conf=", 13) == 0) {
      this->mysql_conf_path = argv[0] + 13;
      if (this->mysql_conf_path.empty()) {
        PMA_EPrintln("ERROR: Failed to set mysql conf path!");
        flags.set(2);
        return;
      }
      flags.set(4);
    } else if (std::strncmp(argv[0], "--sqlite-path=", 14) == 0) {
      this->sqlite_path = argv[0] + 14;
      if (this->sqlite_path.empty()) {
        PMA_EPrintln("ERROR: Failed to set sqlite db filename!");
        flags.set(2);
        return;
      }
    } else if (std::strcmp(argv[0], "--enable-x-real-ip-header") == 0) {
      flags.set(0);
    } else if (std::strncmp(argv[0], "--api-url=", 10) == 0) {
      this->api_url = std::string(argv[0] + 10);
      if (this->api_url.empty()) {
        PMA_EPrintln("ERROR: Failed to parse --api-url=<url> (url is empty)!");
        flags.set(2);
        return;
      }
      if (!this->api_url.ends_with('/')) {
        this->api_url.push_back('/');
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
        PMA_Println(
            "NOTICE: Enabling dangerous \"--enable-override-dest-url\"!");
        flags.set(1);
      } else {
        PMA_EPrintln(
            "ERROR: You must first use \"--important-warning-has-been-read\" "
            "option to enable this option! Please read the documentation to "
            "understand the security implications of this option! It may be "
            "better to just use mulitple \"--addr-port=...\" and "
            "\"--port-to-dest-url=...\" to accomplish the same thing!");
        flags.set(2);
        return;
      }
    } else if (std::strcmp(argv[0], "--important-warning-has-been-read") == 0) {
      PMA_Println(
          "NOTICE: Enabling potentially dangerous options with "
          "--important-warning-has-been-read !");
      flags.set(3);
    } else {
      PMA_EPrintln("ERROR Invalid argument: {}", argv[0]);
      flags.set(2);
      pma_print_usage();
      return;
    }

    --argc;
    ++argv;
  }
}

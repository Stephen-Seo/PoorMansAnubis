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

#include "db_msql_capi.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define READ_BUF_SIZE 256
#define PARAM_SIZE 64

int main(int argc, char **argv) {
  if (argc != 2) {
    puts("./<program> msql.conf");
    return 0;
  }

  char addr_buf[PARAM_SIZE];
  char user_buf[PARAM_SIZE];
  char pass_buf[PARAM_SIZE];
  char dbname_buf[PARAM_SIZE];
  addr_buf[0] = 0;
  user_buf[0] = 0;
  pass_buf[0] = 0;
  dbname_buf[0] = 0;
  uint16_t port = 0;

  FILE *conf_file = fopen(argv[1], "r");
  const char *left = NULL;
  size_t left_size = 0;
  const char *right = NULL;
  size_t right_size = 0;
  // 0 - start of line
  // 1 - wait for =
  // 2 - wait for newline
  int parsing_state = 0;
  int get;

  char read_buf[READ_BUF_SIZE];
  size_t read_buf_idx = 0;
  while (feof(conf_file) == 0) {
    if (read_buf_idx >= READ_BUF_SIZE) {
      puts("ERROR: line in config file exceeds limit!");
      fclose(conf_file);
      return 1;
    } else if (parsing_state == 0) {
      read_buf_idx = 0;
      left = read_buf;
      left_size = 0;
      parsing_state = 1;
      get = fgetc(conf_file);
      if (get == EOF) {
        break;
      } else if (get == '\n' || get == '\r') {
        parsing_state = 0;
        continue;
      } else {
        read_buf[read_buf_idx++] = (char)get;
      }
    } else if (parsing_state == 1) {
      get = fgetc(conf_file);
      if (get == EOF) {
        break;
      } else if (get == '\n' || get == '\r') {
        parsing_state = 0;
        continue;
      } else if (get == '=') {
        left_size = read_buf_idx;
        parsing_state = 2;
        right = read_buf + read_buf_idx;
        right_size = 0;
      } else {
        read_buf[read_buf_idx++] = (char)get;
      }
    } else if (parsing_state == 2) {
      get = fgetc(conf_file);
      if (get == EOF) {
        break;
      } else if (get == '\n' || get == '\n') {
        if (read_buf_idx <= left_size) {
          parsing_state = 0;
          continue;
        }
        right_size = read_buf_idx - left_size;
        if (left_size == 4 && strncmp(left, "user", 4) == 0 &&
            right_size < PARAM_SIZE) {
          memcpy(user_buf, right, right_size);
          user_buf[right_size] = 0;
        } else if (left_size == 8 && strncmp(left, "password", 8) == 0 &&
                   right_size < PARAM_SIZE) {
          memcpy(pass_buf, right, right_size);
          pass_buf[right_size] = 0;
        } else if (left_size == 7 && strncmp(left, "address", 7) == 0 &&
                   right_size < PARAM_SIZE) {
          memcpy(addr_buf, right, right_size);
          addr_buf[right_size] = 0;
        } else if (left_size == 4 && strncmp(left, "port", 4) == 0 &&
                   right_size < PARAM_SIZE) {
          uint64_t port_int = 0;
          for (size_t pidx = 0; pidx < right_size; ++pidx) {
            if (right[pidx] >= '0' && right[pidx] <= '9') {
              port_int = port_int * 10 + (uint64_t)(right[pidx] - '0');
            } else {
              puts("ERROR: Invalid port number in config!");
              fclose(conf_file);
              return 2;
            }
          }
          if (port_int > 0xFFFF) {
            puts("ERROR: Port number too large!");
            fclose(conf_file);
            return 3;
          } else {
            port = (uint16_t)port_int;
          }
        } else if (left_size == 8 && strncmp(left, "database", 8) == 0 &&
                   right_size < PARAM_SIZE) {
          memcpy(dbname_buf, right, right_size);
          dbname_buf[right_size] = 0;
        }
        parsing_state = 0;
      } else {
        read_buf[read_buf_idx++] = (char)get;
      }
    }
  }

  fclose(conf_file);

  // printf("Conf file contents:\n");
  // printf("  user=%s\n", user_buf);
  // printf("  password=%s\n", pass_buf);
  // printf("  address=%s\n", addr_buf);
  // printf("  port=%" PRIu16 "\n", port);
  // printf("  database=%s\n", dbname_buf);

  MSQL_Connection conn =
      MSQL_new(addr_buf, port, user_buf, pass_buf, dbname_buf);
  if (conn == NULL) {
    puts("ERROR: Failed to get MSQL_Connection!");
    return 4;
  }

  MSQL_cleanup(&conn);

  puts("End of program with no errors.");

  return 0;
}

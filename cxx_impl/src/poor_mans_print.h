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

// Note that this header and corresponding source file only exists so that a
// Github Workflow can run unit tests. The "<print>" header exists on and after
// C++23, which "ubuntu-latest" does not have as of 2025-09-26.

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_POOR_MANS_PRINT_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_POOR_MANS_PRINT_H_

#include <cstdio>
#include <format>
#include <string>

#if __cplusplus >= 202302L
#include <print>
#define PMA_Print(fmt, ...) std::print(fmt __VA_OPT__(, ) __VA_ARGS__)
#define PMA_Println(fmt, ...) std::println(fmt __VA_OPT__(, ) __VA_ARGS__)
#define PMA_Println_e() std::println()
#define PMA_EPrint(fmt, ...) std::print(stderr, fmt __VA_OPT__(, ) __VA_ARGS__)
#define PMA_EPrintln(fmt, ...) \
  std::println(stderr, fmt __VA_OPT__(, ) __VA_ARGS__)
#define PMA_EPrintln_e() std::println(stderr, "")
#else
#define PMA_Print(fmt, ...) \
  PoorMans::print_actual(std::format(fmt __VA_OPT__(, ) __VA_ARGS__))
#define PMA_Println(fmt, ...) \
  PoorMans::println_actual(std::format(fmt __VA_OPT__(, ) __VA_ARGS__))
#define PMA_Println_e() PoorMans::println_actual()
#define PMA_EPrint(fmt, ...) \
  PoorMans::eprint_actual(std::format(fmt __VA_OPT__(, ) __VA_ARGS__))
#define PMA_EPrintln(fmt, ...) \
  PoorMans::eprintln_actual(std::format(fmt __VA_OPT__(, ) __VA_ARGS__))
#define PMA_EPrintln_e() PoorMans::eprintln_actual()

namespace std {
void println();
}  // namespace std

namespace PoorMans {

void print_actual(std::string);
void println_actual(std::string);

void eprint_actual(std::string);
void eprintln_actual(std::string);

void println_actual();
void eprintln_actual();

}  // namespace PoorMans

#endif

#endif

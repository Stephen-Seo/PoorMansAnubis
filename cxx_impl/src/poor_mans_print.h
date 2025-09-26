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

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_POOR_MANS_PRINT_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_POOR_MANS_PRINT_H_

#include <cstdio>
#include <format>
#include <string>

#define poormansprintln(fmt, ...) \
  PoorMans::println_actual(std::format(fmt __VA_OPT__(, ) __VA_ARGS__))
#define poormansprint(fmt, ...) \
  PoorMans::print_actual(std::format(fmt __VA_OPT__(, ) __VA_ARGS__))

namespace std {
void println();
}  // namespace std

namespace PoorMans {

template <typename FMT_T, typename... Args>
void print(FMT_T, Args...);

template <typename FMT_T, typename... Args>
void println(FMT_T, Args...);

void println();

void print_actual(std::string);

void println_actual(std::string);

void println_actual();
}  // namespace PoorMans

////////////////////////////////////////////////////////////////////////////////
// Templated Functions Implemenations
////////////////////////////////////////////////////////////////////////////////

template <typename FMT_T, typename... Args>
void PoorMans::print(FMT_T fmt, Args... args) {
  print_actual(std::format(std::forward<FMT_T>(fmt), args...));
}

template <typename FMT_T, typename... Args>
void PoorMans::println(FMT_T fmt, Args... args) {
  println_actual(std::format(std::forward<FMT_T>(fmt), args...));
}

#endif

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

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_CXX_BACKEND_CONSTANTS_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_CXX_BACKEND_CONSTANTS_H_

#include <chrono>
#include <cstdint>

extern const char *HTML_BODY_FACTORS;

extern const char *JS_FACTORS_WORKER;

constexpr unsigned long long REQ_READ_BUF_SIZE = 1024 * 40;

constexpr unsigned int SLEEP_MILLISECONDS = 2;
// 7 seconds
constexpr unsigned int TIMEOUT_ITER_TICKS = 7000 / SLEEP_MILLISECONDS;

constexpr size_t CACHED_TIMEOUT_SECONDS = 120;
constexpr std::chrono::seconds CACHED_TIMEOUT_T =
    std::chrono::seconds(CACHED_TIMEOUT_SECONDS);

constexpr size_t CACHED_CLEAR_SECONDS = 3600;
constexpr std::chrono::seconds CACHED_CLEAR_T =
    std::chrono::seconds(CACHED_CLEAR_SECONDS);

constexpr std::chrono::milliseconds CONN_TRY_LOCK_DURATION =
    std::chrono::milliseconds(500);

constexpr uint32_t DEFAULT_FACTORS_QUADS = 2200;
constexpr uint32_t DEFAULT_JSON_MAX_SIZE = 10000;
constexpr uint32_t ALLOWED_IP_TIMEOUT_MINUTES = 60;
constexpr uint32_t CHALLENGE_FACTORS_TIMEOUT_MINUTES = 1;

constexpr int SOCKET_BACKLOG_SIZE = 2048;

#endif

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

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_HELPERS_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_HELPERS_H_

#include <functional>
#include <optional>

template <typename T>
class GenericCleanup {
  public:
    GenericCleanup(T value, std::function<void(T*)> cleanup_fn);
    ~GenericCleanup();

    GenericCleanup(const GenericCleanup<T> &) = delete;
    GenericCleanup *operator=(const GenericCleanup<T> &) = delete;

    GenericCleanup(GenericCleanup<T> &&);
    GenericCleanup *operator=(GenericCleanup<T> &&);

  private:
    std::optional<std::function<void(T*)>> cleanup_fn;
    std::optional<T> value;
};

namespace PMA_HELPER {

template <unsigned long long SIZE>
extern std::string raw_to_hexadecimal(const std::array<uint8_t, SIZE> &data);

}

////////////////////////////////////////////////////////////////////////////////
// Templated Implementations.
////////////////////////////////////////////////////////////////////////////////

template <typename T>
GenericCleanup<T>::GenericCleanup(T value, std::function<void(T*)> cleanup_fn) :
cleanup_fn(cleanup_fn),
value(value) {}

template <typename T>
GenericCleanup<T>::~GenericCleanup() {
  if (cleanup_fn.has_value() && value.has_value()) {
    cleanup_fn.value()(&value.value());
  }
}

template <typename T>
GenericCleanup<T>::GenericCleanup(GenericCleanup<T> &&other) {
  cleanup_fn = other.cleanup_fn;
  value = other.value;

  other.cleanup_fn = std::nullopt;
  other.value = std::nullopt;
}

template <typename T>
GenericCleanup<T> *GenericCleanup<T>::operator=(GenericCleanup<T> &&other) {
  if (cleanup_fn.has_value() && value.has_value()) {
    cleanup_fn.value()(&value.value());
  }
  cleanup_fn = other.cleanup_fn;
  value = other.value;

  other.cleanup_fn = std::nullopt;
  other.value = std::nullopt;

  return this;
}

template <unsigned long long SIZE>
std::string PMA_HELPER::raw_to_hexadecimal(const std::array<uint8_t, SIZE> &data) {
  std::string hexadecimal;

  for (unsigned long long idx = 0; idx < SIZE; ++idx) {
    uint8_t c;

    c = (data.at(idx) & 0xF0) >> 4;
    if (c >= 0 && c <= 9) {
      c = '0' + c;
    } else {
      c = 'A' + c - 10;
    }
    hexadecimal.push_back(c);

    c = (data.at(idx) & 0xF);
    if (c >= 0 && c <= 9) {
      c = '0' + c;
    } else {
      c = 'A' + c - 10;
    }
    hexadecimal.push_back(c);
  }

  return hexadecimal;
}
#endif

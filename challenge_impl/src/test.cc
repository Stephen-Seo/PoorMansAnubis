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

// Need to access "internal" functions defined in source file.
#include "work2.cc"

#include <atomic>
#include <iostream>

static std::atomic_uint64_t passed(0);
static std::atomic_uint64_t tested(0);

#define CHECK_TRUE(x) \
  if (x) { \
    ++passed; ++tested; \
  } else { \
    ++tested; \
    std::cerr << __LINE__ << ": CHECK_TRUE( " #x " ) Failed!\n"; \
  }

int main() {
    // Test sum_b64
    {
        std::vector<char> first = {'Y', 'e', 's', 'A'};
        std::vector<char> second = {'O', 'k', 'a', 'y'};
        std::vector<char> ret = sum_b64(first, second);

        CHECK_TRUE(ret.size() == 4);
        CHECK_TRUE('m' == ret.at(0));
        CHECK_TRUE('C' == ret.at(1));
        CHECK_TRUE('H' == ret.at(2));
        CHECK_TRUE('z' == ret.at(3));
    }

    {
        std::vector<char> first = {'1', '2', '3', 'a', 'b', 'c'};
        std::vector<char> ret = sum_b64(first, {});

        CHECK_TRUE(ret.size() == 6);
        CHECK_TRUE(ret.at(0) == '1');
        CHECK_TRUE(ret.at(1) == '2');
        CHECK_TRUE(ret.at(2) == '3');
        CHECK_TRUE(ret.at(3) == 'a');
        CHECK_TRUE(ret.at(4) == 'b');
        CHECK_TRUE(ret.at(5) == 'c');

        ret = sum_b64(first, {'B', 'C', 'D'});

        CHECK_TRUE(ret.size() == 6);
        CHECK_TRUE(ret.at(0) == '2');
        CHECK_TRUE(ret.at(1) == '4');
        CHECK_TRUE(ret.at(2) == '6');
        CHECK_TRUE(ret.at(3) == 'a');
        CHECK_TRUE(ret.at(4) == 'b');
        CHECK_TRUE(ret.at(5) == 'c');
    }

    // Test mult_b64
    {
        std::vector<char> first = {'K'};
        std::vector<char> second = {'F'};
        std::vector<char> ret = mult_b64(first, second);

        CHECK_TRUE(ret.size() == 1);
        CHECK_TRUE(ret.at(0) == 'y');
    }

    {
        std::vector<char> first = {'o'};
        std::vector<char> second = {'8'};
        std::vector<char> ret = mult_b64(first, second);

        CHECK_TRUE(ret.size() == 2);
        CHECK_TRUE(ret.at(0) == 'g');
        CHECK_TRUE(ret.at(1) == 'l');
        std::cerr << std::endl;
    }

    {
        std::vector<char> first = {'1', '2', '3'};
        std::vector<char> second = {'a', 'b', 'c'};
        std::vector<char> ret = mult_b64(first, second);

        CHECK_TRUE(ret.size() == 6);
        CHECK_TRUE(ret.at(0) == 'i');
        CHECK_TRUE(ret.at(1) == 'o');
        CHECK_TRUE(ret.at(2) == 'A');
        CHECK_TRUE(ret.at(3) == '6');
        CHECK_TRUE(ret.at(4) == 'z');
        CHECK_TRUE(ret.at(5) == 'Y');
    }

    // Test sum_b64_scalar
    {
        std::vector<char> vec{'a', 'b'};
        sum_b64_scalar(vec, 'B');

        CHECK_TRUE(vec.size() == 2);
        CHECK_TRUE(vec.at(0) == 'b');
        CHECK_TRUE(vec.at(1) == 'b');
    }

    // Test mult_b64_scalar
    {
        std::vector<char> vec{'B', 'C', 'D', 'E'};
        mult_b64_scalar(vec, 3);

        CHECK_TRUE(vec.size() == 4);
        CHECK_TRUE(vec.at(0) == 'D');
        CHECK_TRUE(vec.at(1) == 'G');
        CHECK_TRUE(vec.at(2) == 'J');
        CHECK_TRUE(vec.at(3) == 'M');
    }

    {
        std::vector<char> vec{'H', 'M', 'c', 'r', 'T', '3', 's'};
        mult_b64_scalar(vec, 30);

        CHECK_TRUE(vec.size() == 8);
        CHECK_TRUE(vec.at(0) == 'S');
        CHECK_TRUE(vec.at(1) == 'r');
        CHECK_TRUE(vec.at(2) == 'N');
        CHECK_TRUE(vec.at(3) == 'X');
        CHECK_TRUE(vec.at(4) == 'O');
        CHECK_TRUE(vec.at(5) == '7');
        CHECK_TRUE(vec.at(6) == 'B');
        CHECK_TRUE(vec.at(7) == 'V');
    }

    std::cerr << "Passed: " << passed.load() << "\n";
    std::cerr << "Tested: " << tested.load() << std::endl;
    return passed.load() == tested.load() ? 0 : 1;
}

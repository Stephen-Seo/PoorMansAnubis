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

#include "random.h"

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

    // Test base64_data_to_base64.
    {
        unsigned long long size = 0;
        char *ret = base64_data_to_base64("t", 1, &size);
        CHECK_TRUE(size == 4);
        CHECK_TRUE(strcmp(ret, "dA==") == 0);
        free(ret);

        ret = base64_data_to_base64("te", 2, &size);
        CHECK_TRUE(size == 4);
        CHECK_TRUE(strcmp(ret, "dGU=") == 0);
        free(ret);

        ret = base64_data_to_base64("tes", 3, &size);
        CHECK_TRUE(size == 4);
        CHECK_TRUE(strcmp(ret, "dGVz") == 0);
        free(ret);

        ret = base64_data_to_base64("test", 4, &size);
        CHECK_TRUE(size == 8);
        CHECK_TRUE(strcmp(ret, "dGVzdA==") == 0);
        free(ret);
    }
    // Test base64_base64_to_data.
    {
        unsigned long long size = 0;
        unsigned char *ret = base64_base64_to_data("threetwoonc=", 12, &size);
        CHECK_TRUE(size == 8);
        CHECK_TRUE(ret);
        if (ret) {
            CHECK_TRUE(ret[0] == 0xb6);
            CHECK_TRUE(ret[1] == 0x1a);
            CHECK_TRUE(ret[2] == 0xde);
            CHECK_TRUE(ret[3] == 0x7a);
            CHECK_TRUE(ret[4] == 0xdc);
            CHECK_TRUE(ret[5] == 0x28);
            CHECK_TRUE(ret[6] == 0xa2);
            CHECK_TRUE(ret[7] == 0x77);
            free(ret);
        }

        ret = base64_base64_to_data("SevenEightNineTenOK+", 20, &size);
        CHECK_TRUE(size == 15);
        CHECK_TRUE(ret);
        if (ret) {
            CHECK_TRUE(ret[0] == 0x49);
            CHECK_TRUE(ret[1] == 0xeb);
            CHECK_TRUE(ret[2] == 0xde);
            CHECK_TRUE(ret[3] == 0x9c);
            CHECK_TRUE(ret[4] == 0x48);
            CHECK_TRUE(ret[5] == 0xa0);
            CHECK_TRUE(ret[6] == 0x86);
            CHECK_TRUE(ret[7] == 0xd3);
            CHECK_TRUE(ret[8] == 0x62);
            CHECK_TRUE(ret[9] == 0x9d);
            CHECK_TRUE(ret[10] == 0xe4);
            CHECK_TRUE(ret[11] == 0xde);
            CHECK_TRUE(ret[12] == 0x9c);
            CHECK_TRUE(ret[13] == 0xe2);
            CHECK_TRUE(ret[14] == 0xbe);
            free(ret);
        }

        ret = base64_base64_to_data("Zero123Four567EightNine10Q==", 28, &size);
        CHECK_TRUE(size == 19);
        CHECK_TRUE(ret);
        if (ret) {
            CHECK_TRUE(ret[0] == 0x65);
            CHECK_TRUE(ret[1] == 0xea);
            CHECK_TRUE(ret[2] == 0xe8);
            CHECK_TRUE(ret[3] == 0xd7);
            CHECK_TRUE(ret[4] == 0x6d);
            CHECK_TRUE(ret[5] == 0xc5);
            CHECK_TRUE(ret[6] == 0xa2);
            CHECK_TRUE(ret[7] == 0xea);
            CHECK_TRUE(ret[8] == 0xf9);
            CHECK_TRUE(ret[9] == 0xeb);
            CHECK_TRUE(ret[10] == 0xb1);
            CHECK_TRUE(ret[11] == 0x22);
            CHECK_TRUE(ret[12] == 0x82);
            CHECK_TRUE(ret[13] == 0x1b);
            CHECK_TRUE(ret[14] == 0x4d);
            CHECK_TRUE(ret[15] == 0x8a);
            CHECK_TRUE(ret[16] == 0x77);
            CHECK_TRUE(ret[17] == 0xb5);
            CHECK_TRUE(ret[18] == 0xd1);
        }
        free(ret);
    }

    // Previous tests on base64 <-> data.
    {
        const char *b64 = "dA==";
        unsigned long long size = 0;
        unsigned char *ret = base64_base64_to_data(b64, 4, &size);
        CHECK_TRUE(size == 1);
        CHECK_TRUE(ret);
        if (ret) {
            CHECK_TRUE(strncmp((const char*)ret, "t", 1) == 0);
            free(ret);
        }
    }
    {
        const char *b64 = "dGU=";
        unsigned long long size = 0;
        unsigned char *ret = base64_base64_to_data(b64, 4, &size);
        CHECK_TRUE(size == 2);
        CHECK_TRUE(ret);
        if (ret) {
            CHECK_TRUE(strncmp((const char*)ret, "te", 2) == 0);
            free(ret);
        }
    }
    {
        const char *b64 = "dGVz";
        unsigned long long size = 0;
        unsigned char *ret = base64_base64_to_data(b64, 4, &size);
        CHECK_TRUE(size == 3);
        CHECK_TRUE(ret);
        if (ret) {
            CHECK_TRUE(strncmp((const char*)ret, "tes", 3) == 0);
            free(ret);
        }
    }
    {
        const char *b64 = "dGVzdA==";
        unsigned long long size = 0;
        unsigned char *ret = base64_base64_to_data(b64, 8, &size);
        CHECK_TRUE(size == 4);
        CHECK_TRUE(ret);
        if (ret) {
            CHECK_TRUE(strncmp((const char*)ret, "test", 4) == 0);
            free(ret);
        }
    }
    {
        const unsigned char data[8] = {
            0xb6,
            0x1a,
            0xde,
            0x7a,
            0xdc,
            0x28,
            0xa2,
            0x77
        };
        const char *s_data = (const char*)data;
        unsigned long long size = 0;
        char *b64_ret = base64_data_to_base64(s_data, 8, &size);
        CHECK_TRUE(size == 12);
        CHECK_TRUE(b64_ret);
        if (b64_ret) {
            CHECK_TRUE(strcmp(b64_ret, "threetwoonc=") == 0);
            free(b64_ret);
        }
    }
    {
        const unsigned char data[15] = {
            0x49,
            0xeb,
            0xde,
            0x9c,
            0x48,
            0xa0,
            0x86,
            0xd3,
            0x62,
            0x9d,
            0xe4,
            0xde,
            0x9c,
            0xe2,
            0xbe
        };
        const char *s_data = (const char*)data;
        unsigned long long size = 0;
        char *b64_ret = base64_data_to_base64(s_data, 15, &size);
        CHECK_TRUE(size == 20);
        CHECK_TRUE(b64_ret);
        if (b64_ret) {
            CHECK_TRUE(strcmp(b64_ret, "SevenEightNineTenOK+") == 0);
            free(b64_ret);
        }
    }

    {
        const unsigned char data[19] = {
            0x65, 0xea, 0xe8, 0xd7, 0x6d, 0xc5, 0xa2, 0xea, 0xf9, 0xeb, 0xb1,
            0x22, 0x82, 0x1b, 0x4d, 0x8a, 0x77, 0xb5, 0xd1
        };
        const char *s_data = (const char*)data;
        unsigned long long size = 0;
        char *b64_ret = base64_data_to_base64(s_data, 19, &size);
        CHECK_TRUE(size == 28);
        CHECK_TRUE(b64_ret);
        if (b64_ret) {
            CHECK_TRUE(strcmp(b64_ret, "Zero123Four567EightNine10Q==") == 0);
            free(b64_ret);
        }
    }

    // test random dist.
    {
        void *r_state = rand_state_init();

        for (int idx = 0; idx < 10; ++idx) {
            printf("Random value (0-100): %3d\n",
                   rand_int_range(r_state, 0, 100));
        }

        rand_state_cleanup(r_state);
    }

    std::cerr << "Passed: " << passed.load() << "\n";
    std::cerr << "Tested: " << tested.load() << std::endl;
    return passed.load() == tested.load() ? 0 : 1;
}

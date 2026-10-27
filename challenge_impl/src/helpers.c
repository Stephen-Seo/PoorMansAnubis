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

#include "helpers.h"

#include <math.h>

void solve_quadratic(float a, float b, float c, float *out0, float *out1) {
    if (out0) {
        *out0 = (-b + (float)sqrt(b*b - 4.0F*a*c)) / 2.0F / a;
    }
    if (out1) {
        *out1 = (-b - (float)sqrt(b*b - 4.0F*a*c)) / 2.0F / a;
    }
}

int float_approximate(float a, float b, float margin) {
    if (a > b) {
        if (a - margin < b && a + margin > b) {
            return 0;
        }
    } else if (b - margin < a && b + margin > a) {
        return 0;
    }

    return 1;
}

int float_approximate_default(float a, float b) {
    return float_approximate(a, b, 0.000000001F);
}

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

#include "i_work.h"

#include <stdlib.h>

InteractiveChallenge i_challenge_generate(void) {
    InteractiveChallenge c;

    c.challenge_html = NULL;
    c.client_resp_html = NULL;
    c.answer = NULL;

    return c;
}

void i_challenge_cleanup(InteractiveChallenge challenge) {
    if (challenge.challenge_html) {
        free(challenge.challenge_html);
    }
    if (challenge.client_resp_html) {
        free(challenge.client_resp_html);
    }
    if (challenge.answer) {
        free(challenge.answer);
    }
}

void i_challenge_cleanup_ptr(InteractiveChallenge *challenge) {
    i_challenge_cleanup(*challenge);

    challenge->challenge_html = NULL;
    challenge->client_resp_html = NULL;
    challenge->answer = NULL;
}

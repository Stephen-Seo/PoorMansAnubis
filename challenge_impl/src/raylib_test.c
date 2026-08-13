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

#include <raylib.h>

#include <stdio.h>
#include <stdlib.h>

#include "base64.h"

void test_raylib(void) {
    fprintf(stderr, "Testing raylib...\n");

    InitWindow(100, 100, "test");

    BeginDrawing();
    ClearBackground(WHITE);
    DrawText("Test text.", 2, 20, 12, BLACK);
    EndDrawing();

    Image screen_img = LoadImageFromScreen();
    ImageFlipVertical(&screen_img);
    int size = 0;
    unsigned char *img_data = ExportImageToMemory(screen_img, ".png", &size);
    UnloadImage(screen_img);

    if (img_data && size > 0) {
        unsigned long long out_size = 0;
        char *b64 = base64_data_to_base64((const char*)img_data, (unsigned long long)size, &out_size);
        if (b64 && out_size > 0) {
            printf("image/png in base64:\n%.*s\n", (int)out_size, b64);
            free(b64);
        }
        MemFree(img_data);
    }

    CloseWindow();
}

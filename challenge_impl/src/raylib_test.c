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

#include "raylib_test.h"

extern const unsigned char _binary_Jupiteroid_ttf_start[];
extern const unsigned char _binary_Jupiteroid_ttf_end[];

void test_raylib(void) {
    fprintf(stderr, "Testing raylib...\n");

    InitWindow(300, 150, "test");

    //Font f = LoadFontFromMemory(".ttf",
    //                            _tmp_QuinqueFive_ttf,
    //                            _tmp_QuinqueFive_ttf_len,
    //                            15,
    //                            NULL,
    //                            0);
    Font f = { 0 };
    {
        const size_t font_size =
            (size_t)(_binary_Jupiteroid_ttf_end
                     - _binary_Jupiteroid_ttf_start);
        // Font loading adapted from raylib's rtext.c "LoadFontFromMemory"
        // to load without anti-aliasing.
        f.baseSize = 40;
        f.glyphPadding = 1;
        f.glyphs = LoadFontData(
            _binary_Jupiteroid_ttf_start,
            (int)font_size,
            f.baseSize,
            NULL,
            0,
            FONT_BITMAP,
            &f.glyphCount);

        Image atlas = GenImageFontAtlas(f.glyphs,
                                        &f.recs,
                                        f.glyphCount,
                                        f.baseSize,
                                        1,
                                        0);
        f.texture = LoadTextureFromImage(atlas);

        for (int i = 0; i < f.glyphCount; ++i) {
            UnloadImage(f.glyphs[i].image);
            f.glyphs[i].image = ImageFromImage(atlas, f.recs[i]);
        }

        UnloadImage(atlas);
    }

    BeginDrawing();
    ClearBackground(WHITE);

    if (IsFontValid(f)) {
        fprintf(stderr, "Using Jupiteroid font...\n");
        DrawTextEx(f,
                   "Jupiteroid\nTest\ntext.",
                   (Vector2){2, 2},
                   40.0F,
                   1.0F,
                   BLACK);
        DrawTextEx(f,
                   "Jupiteroid\nTest\ntext.",
                   (Vector2){170, 50},
                   20.0F,
                   1.0F,
                   BLACK);
        UnloadFont(f);
    } else {
        DrawText("Default\nTest\ntext.", 2, 2, 40, BLACK);
    }

    EndDrawing();

    Image screen_img = LoadImageFromScreen();
    ImageFlipVertical(&screen_img);
    int size = 0;
    unsigned char *img_data = ExportImageToMemory(screen_img, ".png", &size);
    UnloadImage(screen_img);

    if (img_data && size > 0) {
        unsigned long long out_size = 0;
        char *b64 = base64_data_to_base64((const char*)img_data,
                                          (unsigned long long)size,
                                          &out_size);
        if (b64 && out_size > 0) {
            printf("image/png in base64:\n%.*s\n", (int)out_size, b64);
            free(b64);
        }
        MemFree(img_data);
    }

    CloseWindow();
}

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

extern const unsigned char _binary_QuinqueFive_ttf_start[];
extern const unsigned char _binary_QuinqueFive_ttf_end[];

const char *QuinqueFive_LICENSE_p1 =
"Copyright © 2019-2025 GGBotNet (https://ggbot.net), with Reserved Font Name \"QuinqueFive\".\n"
"\n"
"This Font Software is licensed under the SIL Open Font License, Version 1.1.\n"
"This license is copied below, and is also available with a FAQ at:\n"
"http://scripts.sil.org/OFL\n"
"\n"
"-----------------------------------------------------------\n"
"SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007\n"
"-----------------------------------------------------------\n"
"\n"
"PREAMBLE\n"
"The goals of the Open Font License (OFL) are to stimulate worldwide\n"
"development of collaborative font projects, to support the font creation\n"
"efforts of academic and linguistic communities, and to provide a free and\n"
"open framework in which fonts may be shared and improved in partnership\n"
"with others.\n"
"\n"
"The OFL allows the licensed fonts to be used, studied, modified and\n"
"redistributed freely as long as they are not sold by themselves. The\n"
"fonts, including any derivative works, can be bundled, embedded, \n"
"redistributed and/or sold with any software provided that any reserved\n"
"names are not used by derivative works. The fonts and derivatives,\n"
"however, cannot be released under any other type of license. The\n"
"requirement for fonts to remain under this license does not apply\n"
"to any document created using the fonts or their derivatives.\n"
"\n"
"DEFINITIONS\n"
"\"Font Software\" refers to the set of files released by the Copyright\n"
"Holder(s) under this license and clearly marked as such. This may\n"
"include source files, build scripts and documentation.\n"
"\n"
"\"Reserved Font Name\" refers to any names specified as such after the\n"
"copyright statement(s).\n"
"\n"
"\"Original Version\" refers to the collection of Font Software components as\n"
"distributed by the Copyright Holder(s).\n"
"\n"
"\"Modified Version\" refers to any derivative made by adding to, deleting,\n"
"or substituting -- in part or in whole -- any of the components of the\n"
"Original Version, by changing formats or by porting the Font Software to a\n"
"new environment.\n"
"\n"
"\"Author\" refers to any designer, engineer, programmer, technical\n"
"writer or other person who contributed to the Font Software.\n"
"\n";

const char *QuinqueFive_LICENSE_p2 =
"PERMISSION & CONDITIONS\n"
"Permission is hereby granted, free of charge, to any person obtaining\n"
"a copy of the Font Software, to use, study, copy, merge, embed, modify,\n"
"redistribute, and sell modified and unmodified copies of the Font\n"
"Software, subject to the following conditions:\n"
"\n"
"1) Neither the Font Software nor any of its individual components,\n"
"in Original or Modified Versions, may be sold by itself.\n"
"\n"
"2) Original or Modified Versions of the Font Software may be bundled,\n"
"redistributed and/or sold with any software, provided that each copy\n"
"contains the above copyright notice and this license. These can be\n"
"included either as stand-alone text files, human-readable headers or\n"
"in the appropriate machine-readable metadata fields within text or\n"
"binary files as long as those fields can be easily viewed by the user.\n"
"\n"
"3) No Modified Version of the Font Software may use the Reserved Font\n"
"Name(s) unless explicit written permission is granted by the corresponding\n"
"Copyright Holder. This restriction only applies to the primary font name as\n"
"presented to the users.\n"
"\n"
"4) The name(s) of the Copyright Holder(s) or the Author(s) of the Font\n"
"Software shall not be used to promote, endorse or advertise any\n"
"Modified Version, except to acknowledge the contribution(s) of the\n"
"Copyright Holder(s) and the Author(s) or with their explicit written\n"
"permission.\n"
"\n"
"5) The Font Software, modified or unmodified, in part or in whole,\n"
"must be distributed entirely under this license, and must not be\n"
"distributed under any other license. The requirement for fonts to\n"
"remain under this license does not apply to any document created\n"
"using the Font Software.\n"
"\n"
"TERMINATION\n"
"This license becomes null and void if any of the above conditions are\n"
"not met.\n"
"\n"
"DISCLAIMER\n"
"THE FONT SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND,\n"
"EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF\n"
"MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT\n"
"OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE\n"
"COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,\n"
"INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL\n"
"DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING\n"
"FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM\n"
"OTHER DEALINGS IN THE FONT SOFTWARE.";

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
            (size_t)(_binary_QuinqueFive_ttf_end
                     - _binary_QuinqueFive_ttf_start);
        // Font loading adapted from raylib's rtext.c "LoadFontFromMemory"
        // to load without anti-aliasing.
        f.baseSize = 40;
        f.glyphPadding = 1;
        f.glyphs = LoadFontData(
            _binary_QuinqueFive_ttf_start,
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
        fprintf(stderr, "Using QuinqueFive font...\n");
        fprintf(stderr,
                "QuinqueFive font is licensed under the OFL 1.1: \n%s%s\n",
                QuinqueFive_LICENSE_p1,
                QuinqueFive_LICENSE_p2);
        DrawTextEx(f,
                   "QinqueFive\nTest\ntext.",
                   (Vector2){2, 2},
                   40.0F,
                   1.0F,
                   BLACK);
        DrawTextEx(f,
                   "QinqueFive\nTest\ntext.",
                   (Vector2){170, 50},
                   10.0F,
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

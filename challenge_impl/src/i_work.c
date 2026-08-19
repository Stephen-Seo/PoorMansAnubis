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
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#include <raylib.h>

#include "base64.h"
#include "random.h"

#define GET_IMAGE_PIXEL(image, x, y, pixel_size) \
    GetPixelColor(((char*)(image).data) \
                    + ((int)(x)) * pixel_size \
                    + ((int)(y)) * pixel_size * (image).width, \
                  (image).format)
#define SET_IMAGE_PIXEL(image, color, x, y, pixel_size) \
    SetPixelColor(((char*)(image).data) \
                    + ((int)(x)) * pixel_size \
                    + ((int)(y)) * pixel_size * (image).width, \
                  color, \
                  (image).format)

#define ROTATION_VARIANCE 35
#define MAX_SIZE_MULTIPLIER_DIAG 0.70710678118654752440F * 0.8F

extern const unsigned char _binary_QuinqueFive_ttf_start[];
extern const unsigned char _binary_QuinqueFive_ttf_end[];

void draw_obscuring_circle(Color color,
                           float interval,
                           Image *image,
                           void *r_state) {
    int center_x = image->width / 2;
    int center_y = image->height / 2;
    int offset_x = rand_int_range(r_state, 0, image->width);
    int offset_y = rand_int_range(r_state, 0, image->height);

    int max_x = offset_x > center_x ? offset_x : center_x * 2 - offset_x;
    int max_y = offset_y > center_y ? offset_y : center_y * 2 - offset_y;

    float max_size = (float)sqrt((float)(max_x * max_x * 4)
                                 + (float)(max_y * max_y * 4));

    for (;max_size >= 4.0F; max_size -= interval) {
        ImageDrawCircleLines(
            image,
            offset_x,
            offset_y,
            (int)(max_size + 0.5F),
            color);
    }
}

void draw_obscuring_grid(Color color,
                         float interval,
                         float offset_x,
                         float offset_y,
                         Image *image) {
    while (offset_x > 0.0F) {
        offset_x -= interval;
    }
    offset_x += interval;
    while (offset_y > 0.0F) {
        offset_y -= interval;
    }
    offset_y += interval;

    for (; offset_x < (float)image->width + interval; offset_x += interval) {
        ImageDrawLine(
            image,
            (int)(offset_x + 0.5F),
            0,
            (int)(offset_x + 0.5F),
            image->height,
            color);
    }

    for (; offset_y < (float)image->height + interval; offset_y += interval) {
        ImageDrawLine(
            image,
            0,
            (int)(offset_y + 0.5F),
            image->width,
            (int)(offset_y + 0.5F),
            color);
    }
}

float inv_sq_lerp(float amt) {
    if (amt < 0.0F) {
        return 0.0F;
    } else if (amt > 1.0F) {
        return 1.0F;
    }

    float minus_one = amt - 1.0F;

    return -((minus_one) * minus_one) + 1.0F;
}

float sq_lerp(float amt) {
    if (amt < 0.0F) {
        return 0.0F;
    } else if (amt > 1.0F) {
        return 1.0F;
    }

    return amt * amt;
}

/// Flags:
/// xxxx xxx1 if set use inv_sq_lerp, otherwise use sq_lerp
void draw_distort_circle(float origin_x,
                         float origin_y,
                         float radius,
                         Image image,
                         Color oob,
                         uint_fast32_t flags) {
    Image copy = ImageCopy(image);

    const int px_data_size = GetPixelDataSize(1, 1, copy.format);

    for (int y = 0; y < copy.height; ++y) {
        for (int x = 0; x < copy.width; ++x) {
            const float diff_x = (float)x - origin_x;
            const float diff_y = (float)y - origin_y;

            const float magnitude =
                (float)sqrt(diff_x * diff_x + diff_y * diff_y);

            if (magnitude <= radius) {
                const float amt = magnitude / radius;
                const float new_amt =
                    ((flags & 1) != 0) ? inv_sq_lerp(amt) : sq_lerp(amt);
                const float offset_amt = new_amt * radius;

                const float unit_x = diff_x / magnitude;
                const float unit_y = diff_y / magnitude;

                const float new_x = origin_x + unit_x * offset_amt;
                const float new_y = origin_y + unit_y * offset_amt;

                const int new_x_int = (int)(new_x + 0.5F);
                const int new_y_int = (int)(new_y + 0.5F);

                Color pixel;
                if (new_x_int < 0 || new_x_int >= copy.width
                        || new_y_int < 0 || new_y_int >= copy.height) {
                    pixel = oob;
                } else {
                    pixel = GET_IMAGE_PIXEL(copy,
                                            new_x + 0.5F,
                                            new_y + 0.5F,
                                            px_data_size);
                }
                SET_IMAGE_PIXEL(image, pixel, x, y, px_data_size);
            }
        }
    }

    UnloadImage(copy);
}

Font get_quinque_five_font(void) {
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

    return f;
}

InteractiveChallenge i_challenge_generate(void) {
    void *r_state = rand_state_init();

    InteractiveChallenge c;

    c.challenge_html = NULL;
    c.client_resp_html = NULL;
    c.answer = NULL;

    InitWindow(50, 50, "rendering_window");

    Font f = get_quinque_five_font();

    const Color text_color = (Color){
        (unsigned char)(128 + rand_int_range(r_state, 0, 127)),
        (unsigned char)(128 + rand_int_range(r_state, 0, 127)),
        (unsigned char)(128 + rand_int_range(r_state, 0, 127)),
        255
    };

    const float font_size = 30.0F;

    const char *text = "Apples";

    Vector2 f_size = MeasureTextEx(f, text, font_size, 1.0F);
    f_size.y *= 2.2F;

    const int max_size = f_size.x > f_size.y
                           ? (int)(f_size.x + 4.5F)
                           : (int)(f_size.y + 4.5F);

    const float max_size_diag = (float)max_size * MAX_SIZE_MULTIPLIER_DIAG;

    Image render_image = GenImageColor(max_size, max_size, BLACK);

    Image text_image = ImageTextEx(f, text, font_size, 1.0F, text_color);

    switch (rand_int_range(r_state, 0, 1)) {
    case 0:
        ImageRotate(&text_image,
                    rand_int_range(r_state,
                                   -ROTATION_VARIANCE,
                                   ROTATION_VARIANCE)
                        + 45);
        break;
    case 1:
        ImageRotate(&text_image,
                    rand_int_range(r_state,
                                   -ROTATION_VARIANCE,
                                   ROTATION_VARIANCE)
                        - 45);
        break;
    }

    ImageDraw(
        &render_image,
        text_image,
        (Rectangle)
            {0, 0, (float)text_image.width, (float)text_image.height},
        (Rectangle)
            {0, 0, (float)render_image.width, (float)render_image.height},
        WHITE);

    UnloadImage(text_image);

    draw_obscuring_circle(text_color, 10.0F, &render_image, r_state);
    draw_obscuring_grid(
        text_color,
        8.0F,
        8.0F - (float)(rand_int_range(r_state, 0, 800)) * 8.0F / 800.0F,
        8.0F - (float)(rand_int_range(r_state, 0, 800)) * 8.0F / 800.0F,
        &render_image);
    float half_max_size = (float)max_size / 2.0F;
    uint_fast32_t distort_circle_flag = 0;
    if (rand_int_range(r_state, 0, 1)) {
        distort_circle_flag |= 1;
    }
    draw_distort_circle(half_max_size,
                        half_max_size,
                        max_size_diag,
                        render_image,
                        BLACK,
                        distort_circle_flag);

    int size = 0;
    unsigned char *img_data = ExportImageToMemory(render_image, ".png", &size);

    UnloadImage(render_image);

    UnloadFont(f);

    CloseWindow();

    if (img_data && size > 0) {
        unsigned long long out_size = 0;
        char *b64 = base64_data_to_base64((const char*)img_data,
                                          (unsigned long long)size,
                                          &out_size);
        if (b64 && out_size > 0) {
#define IMG_PNG_B64_OUTPUT_HTML_FMT "<img src=\"data:image/png;base64,%.*s\" />"
            int html_size = snprintf(
                    NULL,
                    0,
                    IMG_PNG_B64_OUTPUT_HTML_FMT,
                    (int)out_size,
                    b64);
            c.challenge_html = malloc((size_t)(html_size + 1));
            snprintf(c.challenge_html,
                     (size_t)(html_size + 1),
                     IMG_PNG_B64_OUTPUT_HTML_FMT,
                     (int)out_size,
                     b64);

            free(b64);
        }
        MemFree(img_data);
    }

    rand_state_cleanup(r_state);

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

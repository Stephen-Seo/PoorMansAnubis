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
#include <string.h>
#include <time.h>

#include <raylib.h>
#include <external/stb_image_write.h>

#include <data_structures/linked_list.h>

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

#define FONT_SIZE 40.0F
#define MAX_SIZE_PADDING 10.5F
#define ROTATION_VARIANCE 35
#define JPG_QUALITY 20

extern const unsigned char _binary_QuinqueFive_ttf_start[];
extern const unsigned char _binary_QuinqueFive_ttf_end[];

typedef struct jpg_export_part {
    void *data;
    uint64_t size;
} jpg_export_part;

void jpg_export_part_free(void *ptr) {
    if (!ptr) {
        return;
    }

    jpg_export_part *part = (jpg_export_part*)ptr;
    if (part->data) {
        free(part->data);
        part->data = NULL;
    }
    free(part);
}

void jpg_export_part_fn(void *ctx, void *data, int size) {
    if (size <= 0 || !data) {
        return;
    }

    SDArchiverLinkedList *parts = (SDArchiverLinkedList*)ctx;

    jpg_export_part *part = malloc(sizeof(jpg_export_part));
    *part = (jpg_export_part){
        malloc((size_t)size),
        (uint64_t)size
    };

    memcpy(part->data, data, (size_t)size);

    simple_archiver_list_add(parts, part, jpg_export_part_free);
}

/// Returns part.data == NULL on error. Returned data must be FREE'd.
jpg_export_part image_to_jpg_memory(Image image) {
    // Some code adapted from Raylib's rtextures.c .
    int channels = 4;
    uint8_t *data = (uint8_t*)image.data;
    int_fast8_t data_allocated = 0;
    if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) {
        channels = 1;
    } else if (image.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA) {
        channels = 2;
    } else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) {
        channels = 3;
    } else if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        channels = 4;
    } else {
        data = (uint8_t*)LoadImageColors(image);
        data_allocated = 1;
    }

    __attribute__((cleanup(simple_archiver_list_free)))
    SDArchiverLinkedList *parts = simple_archiver_list_init();

    stbi_write_jpg_to_func(jpg_export_part_fn,
                           parts,
                           image.width,
                           image.height,
                           channels,
                           data,
                           JPG_QUALITY);

    if (data_allocated) {
        RL_FREE(data);
    }

    // combine parts of jpg data.
    size_t final_size = 0;

    for (SDArchiverLLNode *node = parts->head->next;
         node != parts->tail;
         node = node->next) {
        jpg_export_part *part = (jpg_export_part*)node->data;
        if (!part || part->size == 0) {
            fprintf(stderr, "to_jpg_memory: Invalid jpg part!\n");
            return (jpg_export_part){NULL, 0};
        }
        final_size += (size_t)part->size;
    }

    if (final_size == 0) {
        fprintf(stderr, "to_jpg_memory: final_size is zero!\n");
        return (jpg_export_part){NULL, 0};
    }

    void *final_data = malloc(final_size);
    size_t idx = 0;
    for (SDArchiverLLNode *node = parts->head->next;
         node != parts->tail;
         node = node->next) {
        jpg_export_part *part = (jpg_export_part*)node->data;
        memcpy(((char*)final_data) + idx,
               part->data,
               part->size);
        idx += (size_t)part->size;
    }

    return (jpg_export_part){final_data, (uint64_t)final_size};
}

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

float sin_lerp(float amt) {
    if (amt < 0.0F) {
        return 0.0F;
    } else if (amt > 1.0F) {
        return 1.0F;
    }

    return (float)sin(amt * 1.57079632679489661922F);
}

float inv_sin_lerp(float amt) {
    if (amt < 0.0F) {
        return 0.0F;
    } else if (amt > 1.0F) {
        return 1.0F;
    }

    return (float)(sin((amt + 3) * 1.57079632679489661922F) + 1.0F);
}

/// Flags:
/// xxxx xxx1 if set use inv_sq_lerp/sin_lerp, else use sq_lerp/inv_sin_lerp
/// xxxx xx1x if set use sq_lerps, else use sin_lerps
void draw_distort_circle(float origin_x,
                         float origin_y,
                         float radius,
                         Image image,
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
                    ((flags & 2) != 0)
                        ? (((flags & 1) != 0)
                            ? inv_sq_lerp(amt) : sq_lerp(amt))
                        : (((flags & 1) != 0)
                            ? sin_lerp(amt) : inv_sin_lerp(amt));

                const float offset_amt = new_amt * radius;

                const float unit_x = diff_x / magnitude;
                const float unit_y = diff_y / magnitude;

                const float new_x = origin_x + unit_x * offset_amt;
                const float new_y = origin_y + unit_y * offset_amt;

                const int new_x_int = (int)(new_x + 0.5F);
                const int new_y_int = (int)(new_y + 0.5F);

                if (new_x_int >= 0 && new_x_int < copy.width
                        && new_y_int >= 0 && new_y_int < copy.height) {
                    Color pixel = GET_IMAGE_PIXEL(copy,
                                            new_x + 0.5F,
                                            new_y + 0.5F,
                                            px_data_size);
                    SET_IMAGE_PIXEL(image, pixel, x, y, px_data_size);
                }
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

    const char *text = "Apples";

    Vector2 f_size = MeasureTextEx(f, text, FONT_SIZE, 1.0F);
    f_size.y *= 2.2F;

    const int max_size = f_size.x > f_size.y
                           ? (int)(f_size.x + MAX_SIZE_PADDING)
                           : (int)(f_size.y + MAX_SIZE_PADDING);

    const float max_size_half = (float)max_size / 2.0F;

    Image render_image = GenImageColor(max_size, max_size, BLACK);

    Image text_image = ImageTextEx(f, text, FONT_SIZE, 1.0F, text_color);

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
    switch (rand_int_range(r_state, 0, 1)) {
    case 0:
        draw_distort_circle(max_size_half,
                            max_size_half,
                            max_size_half,
                            render_image,
                            (uint_fast32_t)rand_int_range(r_state, 0, 3));
        break;
    case 1: {
        const float quarter_max_size = max_size_half / 2.0F;
        draw_distort_circle(quarter_max_size,
                            quarter_max_size,
                            quarter_max_size,
                            render_image,
                            (uint_fast32_t)rand_int_range(r_state, 0, 3));
        draw_distort_circle(quarter_max_size * 3,
                            quarter_max_size,
                            quarter_max_size,
                            render_image,
                            (uint_fast32_t)rand_int_range(r_state, 0, 3));
        draw_distort_circle(quarter_max_size,
                            quarter_max_size * 3,
                            quarter_max_size,
                            render_image,
                            (uint_fast32_t)rand_int_range(r_state, 0, 3));
        draw_distort_circle(quarter_max_size * 3,
                            quarter_max_size * 3,
                            quarter_max_size,
                            render_image,
                            (uint_fast32_t)rand_int_range(r_state, 0, 3));
    }   break;
    }

    jpg_export_part img_data = image_to_jpg_memory(render_image);

    UnloadImage(render_image);

    UnloadFont(f);

    CloseWindow();

    if (img_data.data && img_data.size > 0) {
        unsigned long long out_size = 0;
        char *b64 = base64_data_to_base64((const char*)img_data.data,
                                          (unsigned long long)img_data.size,
                                          &out_size);
        if (b64 && out_size > 0) {
#define IMG_JPG_B64_OUTPUT_HTML_FMT "<img src=\"data:image/jpg;base64,%.*s\" />"
            int html_size = snprintf(
                    NULL,
                    0,
                    IMG_JPG_B64_OUTPUT_HTML_FMT,
                    (int)out_size,
                    b64);
            c.challenge_html = malloc((size_t)(html_size + 1));
            snprintf(c.challenge_html,
                     (size_t)(html_size + 1),
                     IMG_JPG_B64_OUTPUT_HTML_FMT,
                     (int)out_size,
                     b64);

            free(b64);
        }
        free(img_data.data);
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

/* LIBGPU 80023288-800235ac. */
#include "psx/libgpu.h"
u32* FntFlush(int id) {
    psyq_font_window_t* window;
    char* text;
    SPRT_8* sprite;
    u32* ot;
    int remaining;
    int x;
    int y;
    int right;
    int no_wrap;
    int newline;
    int glyph;
    u8 u;
    u8 v;
    int maximum_x = 0;
    int bottom;
    int r = 128;
    int g = 128;
    int b = 128;
    if (id < 0 || id >= g_psyq_gpu_font_window_count) {
        id = g_psyq_gpu_default_font_window;
        if (!Font[id].text)
            return 0;
    }
    window = &Font[id];
    ot = &window->mode.tag;
    text = window->text;
    remaining = window->character_limit;
    x = window->tile.x0;
    y = window->tile.y0;
    right = x + window->tile.w;
    bottom = y + window->tile.h;
    sprite = window->sprites;
    no_wrap = window->wrap_disabled;
    TermPrim(ot);
    while (*text && remaining) {
        newline = 0;
        switch (*text) {
        case '~':
            text++;
            if (*text == 'c') {
                r = (*++text - '0') << 4;
                g = (*++text - '0') << 4;
                b = (*++text - '0') << 4;
            }
            break;
        case '\t':
            x += 32;
            if (x >= right && !no_wrap)
                newline = 1;
            break;
        default:
            if (*text >= 'a' && *text <= 'z')
                glyph = *text - 64;
            else
                glyph = *text - 32;
            u = (glyph % 16) * 8;
            v = (glyph / 16) * 8;
            setUV0(sprite, u, v);
            setXY0(sprite, x, y);
            setRGB0(sprite, r, g, b);
            AddPrim(ot, sprite++);
            x += 8;
            if (x >= right && !no_wrap)
                newline = 1;
            break;
        case ' ':
            x += 8;
            if (x >= right && !no_wrap)
                newline = 1;
            break;
        case '\n':
            newline = 1;
            break;
        }
        if (newline) {
            if (maximum_x < x)
                maximum_x = x;
            y += 8;
            x = window->tile.x0;
            if (y >= bottom)
                break;
        }
        text++;
        remaining--;
    }
    if (window->tile.code) {
        AddPrim(ot, &window->tile);
        if (no_wrap) {
            setWH(&window->tile, maximum_x - window->tile.x0, y + 8 - window->tile.y0);
        }
    }
    DrawOTag(ot);
    window->text_count = 0;
    *window->text = 0;
    return ot;
}

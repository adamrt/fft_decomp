#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/libgpu.h"

void gfx_init_image_loading(
    void* primitive, const point16_t* base_load, const RECT* base_screen, const image_load_parameters_t* parameters) {
    int texture_column;
    SPRT* sprite = primitive;
    POLY_FT4* quad = primitive;

    if ((getcode(primitive) & 0x7c) == 0x64) {
        if (parameters->x_load >= 0) {
            texture_column = base_load->x & 0x3f;
            sprite->u0 = (texture_column << 2) + parameters->x_load;
            sprite->v0 = base_load->y + parameters->y_load;
            sprite->w = parameters->width;
            sprite->h = parameters->height;
        }
        sprite->x0 = base_screen->x + parameters->x_screen_offset;
        sprite->y0 = base_screen->y + parameters->y_screen_offset;
        return;
    }

    if (parameters->x_load >= 0) {
        quad->tpage = GetTPage(0, 0, base_load->x, base_load->y & 0xf00);
        setUV4(quad, parameters->x_load, base_load->y + parameters->y_load, parameters->x_load + parameters->width,
            base_load->y + parameters->y_load, parameters->x_load,
            base_load->y + parameters->y_load + parameters->height, parameters->x_load + parameters->width,
            base_load->y + parameters->y_load + parameters->height);
    }

    setXY4(quad, base_screen->x + parameters->x_screen_offset, base_screen->y + parameters->y_screen_offset,
        base_screen->x + parameters->x_screen_offset + parameters->width, base_screen->y + parameters->y_screen_offset,
        base_screen->x + parameters->x_screen_offset, base_screen->y + parameters->y_screen_offset + parameters->height,
        base_screen->x + parameters->x_screen_offset + parameters->width,
        base_screen->y + parameters->y_screen_offset + parameters->height);
}

void menu_draw_numeric_display_entries(u8* buffer, menu_number_entry_t* entries, glyph_blit_t* position, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        position->rect.x = entries->x;
        position->rect.y = entries->y;
        if (entries->kind == 0) {
            text_draw_large_number_glyphs_variant(*entries->value, entries->format, buffer, position);
        } else if (entries->kind == 1) {
            menu_draw_number_glyphs(*entries->value, entries->format, buffer, position);
        }
        entries++;
    }
}

/* Draw decimal glyphs right to left and clear the text palette afterward. */
void menu_display_number_entry(s32 value, u8 digit_count, u8* image, glyph_blit_t* origin) {
    s16 cell;

    origin->rect.x += (digit_count - 1) * 6;
    do {
        text_render_glyph_to_4bpp_image(
            g_text_glyph_bitmap_data + (value % 10) * TEXT_GLYPH_BYTES, image, origin, g_menu_text_palette_offset);
        value = value / 10;
        cell = origin->rect.x;
        origin->rect.x = cell - 6;
    } while (value != 0);
    g_menu_text_palette_offset = 0;
}

void menu_draw_number_glyphs(s32 value, s32 digits, u8* pixels, glyph_blit_t* pos) {
    s32 i;
    s32 cursor;
    glyph_blit_t* src;

    /* One variable holds the sign-glyph selector first and the x to leave the
     * cursor at afterwards; the target keeps both roles in $s6. */
    src = &g_menu_number_glyph;
    g_menu_number_glyph.rect.h = 0xF;
    cursor = digits & 0xFF00;
    if (cursor == 0xC00) {
        pos->rect.x += 2;
        src->rect.x = 0xBA;
        g_menu_number_glyph.rect.y = 0xD;
        g_menu_number_glyph.rect.w = 6;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
        g_menu_number_glyph.rect.w = 8;
        return;
    }
    if (digits & 0x1000) {
        src->rect.x = 0xC8;
        g_menu_number_glyph.rect.y = 0;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, src, pos);
        pos->rect.x += 7;
    }
    if ((digits & 0x800) || (digits & 0x400)) {
        if (digits & 0x400) {
            src->rect.x = 0xC8;
        } else {
            src->rect.x = 0xBA;
        }
        src->rect.y = 0xD;
        src->rect.w = 6;
        pos->rect.x -= 6;
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_number_glyph, pos);
        pos->rect.x += 6;
        src->rect.w = 8;
    }
    /* Not a variable: the target recomputes the digit count at the loop test
     * rather than keeping it live, so each use spells the mask out. */
    pos->rect.x += ((digits & 0xFF) - 1) * 7;
    cursor = pos->rect.x + 7;
    for (i = 0; i < (digits & 0xFF); i++) {
        src->rect.y = 0;
        src->rect.x = (value % 10) * 8 + 0x78;
        if (g_menu_hide_numeric_values != 0) {
            src->rect.x = 0x48;
            src->rect.y = 0x10;
        }
        text_blit_glyph(g_menu_glyph_sheet->pixels, pixels, &g_menu_number_glyph, pos);
        /* After the call: the quotient stays live past the argument setup, so
         * it takes a callee-saved register and the update lands in the delay
         * slot, as the target does at 0x238. */
        value /= 10;
        pos->rect.x -= 7;
    }
    pos->rect.x = cursor;
}

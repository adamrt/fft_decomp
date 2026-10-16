/* CARD's own menu commands: texture page, ordering-table entry and
 * zero-padded number. */

#include "fft/event_card.h"
#include "fft/gfx.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/text.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Stream command: set the texture page from the command (color mode
 * data[4], x data[3] * 16, y data[2] * 256, blend rate 0) and return the
 * next command. */
const u8* card_set_texture_page(const u8* data) {
    g_card_gfx_texture_page = GetTPage(data[4], 0, data[3] << 4, data[2] << 8);
    return data + data[1];
}

/* Menu script command: select the ordering-table slot later primitives go to (byte 3) and return
 * the next command (byte 1 holds this command's length). */
const u8* card_set_draw_priority(const u8* data) {
    g_menu_gfx_otag_index = data[3];
    return data + data[1];
}

/* Draw the command's literal value as `width` zero-padded digit glyphs from (x, y),
 * `spacing` pixels apart, tinted with the menu text color. A style of 2 or more draws
 * nothing. Returns the next command. */
menu_draw_number_command_t* card_draw_decimal_number(menu_draw_number_command_t* command) {
    s8 text[16];
    menu_digit_glyph_t glyph;
    RECT unused_minus_rect; /* Preserves the original frame size. */
    s32 y;
    s16 pad;
    s32 pos;
    s32 value;
    s32 digit;
    s32 i;
    u16 clut;
    const rgb24_t* color;

    value = command->value;
    y = command->y;
    clut = g_menu_alternate_number_clut;
    glyph.u = 0x78;
    glyph.v = 0x10;
    glyph.w = 6;
    glyph.h = 0xB;
    pad = command->width - sprintf(text, g_card_text_decimal_format, value);
    pos = 0;
    color = &g_menu_text_color;
    for (i = 0; i < command->width; i++) {
        glyph.rect.x = command->x + (i * command->spacing);
        glyph.rect.y = y;
        glyph.rect.w = glyph.w;
        glyph.rect.h = glyph.h;
        if (pad != 0) {
            digit = 0;
            pad -= 1;
        } else {
            digit = text[pos] - '0';
            pos += 1;
        }
        if (command->style < 2) {
            menu_gfx_enqueue_textured_quad(&glyph.rect, glyph.u + (glyph.w * digit), glyph.v, color,
                g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
        }
    }
    return (menu_draw_number_command_t*)((u8*)command + command->length);
}

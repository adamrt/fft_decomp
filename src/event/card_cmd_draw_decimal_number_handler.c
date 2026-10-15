#include "fft/event_card.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Draw the literal value as zero-padded digits at the requested position. */
menu_draw_number_command_t* card_cmd_draw_decimal_number_handler(menu_draw_number_command_t* command) {
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

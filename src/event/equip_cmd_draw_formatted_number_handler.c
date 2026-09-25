#include "fft/event_equip.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Destination rect, source glyph cell and minus-sign rect. The target keeps
 * the cell fields in the frame and reloads them inside the digit loop, so they
 * form one address-taken aggregate. */
typedef struct equip_number_glyph {
    RECT rect;
    s16 u;
    s16 v;
    u16 w;
    u16 h;
    RECT minus_rect;
} equip_number_glyph_t;

/* Render command that draws the decimal value returned by one of the list-row
 * callbacks as a row of digit tiles.
 *
 * Twin of JOBSTTS jobstts_menu_script_draw_formatted_number, extended with a
 * leading minus sign and three glyph styles (data[2]); style 2 uses the
 * two-row digit strip with its own CLUT. `negative` is s16: the target copies
 * the promoted halfword into a scratch register before each test. Clearing it
 * after the sign quad's call (not before) reproduces the target schedule. */
u8* equip_cmd_draw_formatted_number_handler(u8* data) {
    s8 text[16];
    equip_number_glyph_t glyph;
    equip_row_callback_t callback;
    s32 arg;
    s32 y;
    s32 value;
    s16 negative;
    s32 cell;
    s32 dash_x;
    s32 cell_x;
    s32 i;
    s16 pad;
    s32 pos;
    s32 digit;
    u16 clut;

    callback = g_equip_menu_list_row_callbacks[data[3]];
    negative = 0;
    if (g_equip_menu_list_row_mode == 0) {
        arg = data[9];
        y = data[8];
    } else {
        arg = g_equip_menu_scroll_base_index + g_equip_menu_list_row_index;
        y = ((g_equip_menu_list_row_height * g_equip_menu_list_row_index) + data[8]) - g_equip_menu_list_scroll_offset;
        if (g_equip_menu_list_scroll_offset < 0) {
            arg -= 1;
            y -= g_equip_menu_list_row_height;
        }
    }
    value = callback(arg);
    if (value < 0) {
        value = -value;
        negative = 1;
    }
    if (data[6] != 0) {
        clut = g_equip_text_metric_1;
    } else {
        clut = g_equip_text_metric_2;
    }
    if (value & 0x40000000) {
        clut = g_equip_text_metric_3;
        value &= ~0x40000000;
    }
    if (data[2] == 1) {
        glyph.u = 0x78;
        glyph.v = 1;
        glyph.w = 8;
        glyph.h = 0xE;
    } else if (data[2] == 0) {
        glyph.u = 0x78;
        glyph.v = 0x10;
        glyph.w = 6;
        glyph.h = 0xB;
    } else {
        glyph.u = 0x20;
        glyph.v = 0;
        glyph.w = 0xA;
        glyph.h = 0x10;
    }
    if (value & 0x20000000) {
        value = data[4];
        while (value > 0) {
            cell = value - 1;
            dash_x = data[7] + (cell * data[5]);
            glyph.rect.y = y + 5;
            glyph.rect.w = 6;
            glyph.rect.h = 3;
            glyph.rect.x = dash_x - 1;
            equip_gfx_enqueue_textured_quad(&glyph.rect, 0xBA, 0x13, 0, g_equip_gfx_semitransparency,
                g_equip_text_digit_texture_page, clut, g_equip_gfx_sprite_ot_index);
            value = cell;
        }
        return data + data[1];
    }
    pad = data[4] - sprintf((char*)text, g_equip_text_decimal_format, value) - negative;
    pos = 0;
    for (i = 0; i < data[4]; i++) {
        cell_x = data[7] + (i * data[5]);
        glyph.rect.w = glyph.w;
        glyph.rect.y = y;
        glyph.rect.h = glyph.h;
        glyph.rect.x = cell_x;
        if (negative != 0) {
            glyph.minus_rect.x = cell_x - 6;
            glyph.minus_rect.y = y + 2;
            glyph.minus_rect.w = 6;
            glyph.minus_rect.h = 0xB;
            equip_gfx_enqueue_textured_quad(&glyph.minus_rect, 0xBA, 0x10, 0, g_equip_gfx_semitransparency,
                g_equip_text_digit_texture_page, clut, g_equip_gfx_sprite_ot_index);
            negative = 0;
        } else {
            digit = 0;
            if (pad != 0) {
                pad -= 1;
            } else {
                digit = text[pos] - '0';
                pos += 1;
            }
            if (data[2] < 2) {
                equip_gfx_enqueue_textured_quad(&glyph.rect, glyph.u + ((s16)glyph.w * digit), glyph.v, 0,
                    g_equip_gfx_semitransparency, g_equip_text_digit_texture_page, clut, g_equip_gfx_sprite_ot_index);
            } else {
                if (digit < 8) {
                    glyph.v = 0;
                    glyph.u = (s16)glyph.w * digit + 0x20;
                } else {
                    glyph.v = 0x10;
                    glyph.u = (s16)glyph.w * (digit - 8) + 0x20;
                }
                equip_gfx_enqueue_textured_quad(&glyph.rect, glyph.u, glyph.v, 0, g_equip_gfx_semitransparency,
                    g_equip_text_digit_texture_page, g_equip_text_metric_0, g_equip_gfx_sprite_ot_index);
            }
        }
    }
    return data + data[1];
}

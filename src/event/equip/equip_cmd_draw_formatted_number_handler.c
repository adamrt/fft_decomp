#include "fft/event_equip.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Draw the getter's signed decimal value or dashes in the requested digit style. */
menu_instruction_t* equip_cmd_draw_formatted_number_handler(menu_instruction_t* data) {
    s8 text[16];
    menu_digit_glyph_t glyph;
    RECT minus_rect;
    equip_row_callback_t callback;
    s32 arg;
    s32 y;
    s32 value;
    /* Clear this 16-bit value after drawing the sign to retain the store schedule. */
    s16 negative;
    s32 cell;
    s32 dash_x;
    s32 cell_x;
    s32 i;
    s16 pad;
    s32 pos;
    s32 digit;
    u16 clut;

    callback = g_menu_row_callbacks[data->number.getter].value;
    negative = 0;
    if (g_menu_list_row_mode == 0) {
        arg = data->number.value;
        y = data->number.y;
    } else {
        arg = g_menu_scroll_base_index + g_menu_list_row_index;
        y = ((g_menu_list_row_height * g_menu_list_row_index) + data->number.y) - g_menu_list_scroll_offset;
        if (g_menu_list_scroll_offset < 0) {
            arg -= 1;
            y -= g_menu_list_row_height;
        }
    }
    value = callback(arg);
    if (value < 0) {
        value = -value;
        negative = 1;
    }
    if (data->number.alternate_palette != 0) {
        clut = g_menu_alternate_number_clut;
    } else {
        clut = g_menu_palette_clut;
    }
    if (value & MENU_NUMBER_VALUE_DISABLED) {
        clut = g_menu_disabled_number_clut;
        value &= ~MENU_NUMBER_VALUE_DISABLED;
    }
    if (data->number.style == 1) {
        glyph.u = 0x78;
        glyph.v = 1;
        glyph.w = 8;
        glyph.h = 0xE;
    } else if (data->number.style == 0) {
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
    if (value & MENU_NUMBER_VALUE_DASHES) {
        value = data->number.width;
        while (value > 0) {
            cell = value - 1;
            dash_x = data->number.x + (cell * data->number.spacing);
            glyph.rect.y = y + 5;
            glyph.rect.w = 6;
            glyph.rect.h = 3;
            glyph.rect.x = dash_x - 1;
            menu_gfx_enqueue_textured_quad(&glyph.rect, 0xBA, 0x13, 0, g_menu_gfx_semitransparency,
                g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
            value = cell;
        }
        return menu_next_instruction(data);
    }
    pad = data->number.width - sprintf(text, g_equip_text_decimal_format, value) - negative;
    pos = 0;
    for (i = 0; i < data->number.width; i++) {
        cell_x = data->number.x + (i * data->number.spacing);
        glyph.rect.w = glyph.w;
        glyph.rect.y = y;
        glyph.rect.h = glyph.h;
        glyph.rect.x = cell_x;
        if (negative != 0) {
            minus_rect.x = cell_x - 6;
            minus_rect.y = y + 2;
            minus_rect.w = 6;
            minus_rect.h = 0xB;
            menu_gfx_enqueue_textured_quad(&minus_rect, 0xBA, 0x10, 0, g_menu_gfx_semitransparency,
                g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
            negative = 0;
        } else {
            digit = 0;
            if (pad != 0) {
                pad -= 1;
            } else {
                digit = text[pos] - '0';
                pos += 1;
            }
            if (data->number.style < 2) {
                menu_gfx_enqueue_textured_quad(&glyph.rect, glyph.u + (glyph.w * digit), glyph.v, 0,
                    g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
            } else {
                if (digit < 8) {
                    glyph.v = 0;
                    glyph.u = glyph.w * digit + 0x20;
                } else {
                    glyph.v = 0x10;
                    glyph.u = glyph.w * (digit - 8) + 0x20;
                }
                menu_gfx_enqueue_textured_quad(&glyph.rect, glyph.u, glyph.v, 0, g_menu_gfx_semitransparency,
                    g_menu_atlas_texture_page, g_menu_window_clut, g_menu_gfx_otag_index);
            }
        }
    }
    return menu_next_instruction(data);
}

/* The menu command that draws a row callback's value, or dashes, on the current row. */

#include "fft/event_jobstts.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/text.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Draw the getter's decimal value, or dashes, using the current row and palette. */
menu_instruction_t* jobstts_menu_opcode_0d_draw_formatted_number(menu_instruction_t* data) {
    s8 text[16];
    menu_digit_glyph_t glyph;
    u16 unused[8]; /* Preserves the original frame size. */
    jobstts_condition_t condition;
    s32 arg;
    s32 y;
    s32 value;
    /* Separate positions preserve the load/store order in the two loops. */
    s32 dash_x;
    s32 i;
    s32 pad;
    s32 pos;
    s32 digit;
    s32 cell_x;
    s32 cell;
    u16 clut;

    condition = g_menu_row_callbacks[data->number.getter].value;
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

    value = condition(arg);
    if (data->number.alternate_palette != 0) {
        clut = g_menu_alternate_number_clut;
    } else {
        clut = g_menu_palette_clut;
    }
    if (value & MENU_NUMBER_VALUE_DISABLED) {
        clut = g_menu_disabled_number_clut;
        value &= ~MENU_NUMBER_VALUE_DISABLED;
    }

    glyph.u = 0x78;
    glyph.v = 0x10;
    glyph.w = 6;
    glyph.h = 0xB;

    if (value & MENU_NUMBER_VALUE_DASHES) {
        value = data->number.width;
        while (value > 0) {
            cell = value - 1;
            dash_x = data->number.x + (cell * data->number.spacing);
            glyph.rect.y = y + 5;
            glyph.rect.w = 6;
            glyph.rect.h = 3;
            glyph.rect.x = dash_x;
            menu_gfx_enqueue_textured_quad(&glyph.rect, 0xBA, 0x13, 0, g_menu_gfx_semitransparency,
                g_jobstts_gfx_background_texture_page, clut, g_menu_gfx_otag_index);
            value = cell;
        }
        return menu_next_instruction(data);
    }

    pad = data->number.width - sprintf(text, g_jobstts_text_decimal_format, value);
    pos = 0;
    i = 0;
    if (data->number.width != 0) {
        do {
            if (pad != 0) {
                digit = 0;
                pad -= 1;
            } else {
                digit = text[pos] - 0x30;
                pos += 1;
            }
            cell_x = data->number.x + (i * data->number.spacing);
            glyph.rect.w = glyph.w;
            glyph.rect.y = y;
            glyph.rect.h = glyph.h;
            glyph.rect.x = cell_x;
            menu_gfx_enqueue_textured_quad(&glyph.rect, glyph.u + (glyph.w * digit), glyph.v, 0,
                g_menu_gfx_semitransparency, g_jobstts_gfx_background_texture_page, clut, g_menu_gfx_otag_index);
            i += 1;
        } while (i < data->number.width);
    }
    return menu_next_instruction(data);
}

#include "fft/event_bunit.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Draw a row-callback value as a row of digit glyphs (opcodes 0x19-0x1B).
 *
 * BUNIT twin of world_menu_script_draw_formatted_number: getter/width/spacing/
 * palette/x/y/value come from the command; 0x40000000 in the value selects the
 * highlighted CLUT, 0x20000000 draws dashes, and a negative value gets a minus
 * glyph ahead of the first cell.
 *
 * Register allocation hinges on GCC 2.6.3 allocno priorities: `negative` is
 * an s8 (the promoted subreg gives the target's `move v0,s7` copy before the
 * test) and is cleared after the minus-glyph call, and the if/else spellings
 * of `color` and `value` set the live lengths that put `color` in s6 ahead of
 * `negative` in s7. */
menu_instruction_t* bunit_menu_script_draw_formatted_number(menu_instruction_t* command) {
    s8 text[16];
    RECT rect;
    RECT glyph;
    RECT minus_rect;
    s32 (*getter)(s32);
    s32 row;
    s32 y;
    s32 value;
    s32 digit;
    s16 blanks;
    s32 text_index;
    s32 i;
    s8 negative;
    rgb24_t* color;
    u16 clut;
    s32 x;
    s32 next_remaining;
    u8 opcode;

    getter = g_menu_row_callbacks[command->number.getter].value;
    negative = 0;
    if (g_menu_list_row_mode == 0) {
        row = command->number.value;
        y = command->number.y;
    } else {
        row = g_menu_scroll_base_index + g_menu_list_row_index;
        y = g_bunit_menu_list_row_height * g_menu_list_row_index + command->number.y - g_menu_list_scroll_offset;
        if (g_menu_list_scroll_offset < 0) {
            row--;
            y -= g_bunit_menu_list_row_height;
        }
    }
    if (getter != 0) {
        value = getter(row);
    } else {
        value = row;
    }
    if (value < 0) {
        value = -value;
        negative = 1;
    }
    opcode = command->number.opcode;
    if (opcode == MENU_SCRIPT_OP_NUMBER_TINTED || opcode == MENU_SCRIPT_OP_NUMBER_TINTED_NO_LEADING_ZEROS) {
        color = &g_menu_text_color;
    } else {
        color = 0;
    }
    if (command->number.alternate_palette != 0) {
        clut = g_menu_alternate_number_clut;
    } else {
        clut = g_menu_palette_clut;
    }
    if (value & MENU_NUMBER_VALUE_DISABLED) {
        clut = g_menu_disabled_number_clut;
        value &= ~MENU_NUMBER_VALUE_DISABLED;
    }
    if (command->number.style == 1) {
        glyph.x = 0x78;
        glyph.y = 1;
        glyph.w = 8;
        glyph.h = 0xE;
    } else if (command->number.style == 0) {
        glyph.x = 0x78;
        glyph.y = 0x10;
        glyph.w = 6;
        glyph.h = 0xB;
    } else {
        glyph.x = 0x20;
        glyph.y = 0;
        glyph.w = 0xA;
        glyph.h = 0x10;
    }
    if (value & MENU_NUMBER_VALUE_DASHES) {
        value = command->number.width;
        while (value > 0) {
            next_remaining = value - 1;
            rect.x = command->number.x + next_remaining * command->number.spacing;
            rect.y = y + 5;
            rect.w = 6;
            rect.h = 3;
            menu_gfx_enqueue_textured_quad(&rect, 0xBA, 0x13, color, g_menu_gfx_semitransparency,
                g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
            value = next_remaining;
        }
    } else {
        blanks = command->number.width - sprintf(text, g_bunit_text_decimal_format, value) - negative;
        text_index = 0;
        for (i = 0; i < command->number.width; i++) {
            x = command->number.x + i * command->number.spacing;
            rect.y = y;
            rect.w = glyph.w;
            rect.h = glyph.h;
            rect.x = x;
            if (negative != 0) {
                minus_rect.x = x - 6;
                minus_rect.y = y + 2;
                minus_rect.w = 6;
                minus_rect.h = 0xB;
                menu_gfx_enqueue_textured_quad(&minus_rect, 0xBA, 0x10, color, g_menu_gfx_semitransparency,
                    g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
                negative = 0;
            } else {
                if (command->number.opcode == MENU_SCRIPT_OP_NUMBER_DIM_LEADING_ZEROS) {
                    clut = g_menu_palette_clut;
                }
                if (blanks != 0) {
                    digit = 0;
                    blanks--;
                    if (command->number.opcode == MENU_SCRIPT_OP_NUMBER_TINTED_NO_LEADING_ZEROS) {
                        continue;
                    }
                    if (command->number.opcode == MENU_SCRIPT_OP_NUMBER_DIM_LEADING_ZEROS
                        && i < command->number.width - 1) {
                        clut = g_menu_disabled_number_clut;
                    }
                } else {
                    digit = text[text_index] - '0';
                    text_index++;
                }
                if (command->number.style < 2) {
                    menu_gfx_enqueue_textured_quad(&rect, glyph.x + glyph.w * digit, glyph.y, color,
                        g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
                }
            }
        }
    }
    return menu_next_instruction(command);
}

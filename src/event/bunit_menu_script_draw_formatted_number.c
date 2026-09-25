#include "fft/event_bunit.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Draw a row-callback value as a row of digit glyphs (opcodes 0x19-0x1B).
 *
 * BUNIT twin of world_menu_script_draw_formatted_number: getter/width/spacing/
 * palette/x/y/value come from the command; 0x40000000 in the value selects the
 * highlighted CLUT, 0x20000000 draws dashes, and a negative value gets a minus
 * glyph ahead of the first cell.
 *
 * Register allocation hinges on GCC 2.6.3 allocno priorities: the dash loop
 * is a goto loop (no loop notes, so its uses of `y` and `color` weigh 1),
 * `negative` is an s8 (the promoted subreg gives the target's `move v0,s7`
 * copy before the test) and is cleared after the minus-glyph call, and the
 * if/else spellings of `color` and `value` set the live lengths that put
 * `color` in s6 ahead of `negative` in s7. */
bunit_draw_number_command_t* bunit_menu_script_draw_formatted_number(bunit_draw_number_command_t* command) {
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
    u8* color;
    u16 clut;
    s32 x;
    s32 next_remaining;
    u8 opcode;

    getter = g_bunit_menu_row_callbacks[command->getter];
    negative = 0;
    if (g_bunit_menu_scroll_list_active == 0) {
        row = command->value;
        y = command->y;
    } else {
        row = g_bunit_menu_scroll_base_index + g_bunit_menu_cursor_row;
        y = (s16)g_bunit_menu_list_row_height * g_bunit_menu_cursor_row + command->y - g_bunit_menu_scroll_pixel_offset;
        if (g_bunit_menu_scroll_pixel_offset < 0) {
            row--;
            y -= (s16)g_bunit_menu_list_row_height;
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
    opcode = command->opcode;
    if (opcode == 0x1B || opcode == 0x19) {
        color = g_bunit_gfx_sprite_color;
    } else {
        color = 0;
    }
    if (command->alternate_palette != 0) {
        clut = g_bunit_text_metric_1;
    } else {
        clut = g_bunit_text_metric_2;
    }
    if (value & 0x40000000) {
        clut = g_bunit_text_metric_3;
        value &= 0xBFFFFFFF;
    }
    if (command->style == 1) {
        glyph.x = 0x78;
        glyph.y = 1;
        glyph.w = 8;
        glyph.h = 0xE;
    } else if (command->style == 0) {
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
    if (value & 0x20000000) {
        value = command->width;
        while (value > 0) {
            next_remaining = value - 1;
            rect.x = command->x + next_remaining * command->spacing;
            rect.y = y + 5;
            rect.w = 6;
            rect.h = 3;
            bunit_gfx_enqueue_textured_quad(&rect, 0xBA, 0x13, color, g_bunit_gfx_semitrans_enabled,
                g_bunit_text_digit_texture_page, clut, g_bunit_gfx_otag_index);
            value = next_remaining;
        }
    } else {
        blanks = command->width - sprintf((char*)text, g_bunit_text_decimal_format, value) - negative;
        text_index = 0;
        for (i = 0; i < command->width; i++) {
            x = command->x + i * command->spacing;
            rect.y = y;
            rect.w = glyph.w;
            rect.h = glyph.h;
            rect.x = x;
            if (negative != 0) {
                minus_rect.x = x - 6;
                minus_rect.y = y + 2;
                minus_rect.w = 6;
                minus_rect.h = 0xB;
                bunit_gfx_enqueue_textured_quad(&minus_rect, 0xBA, 0x10, color, g_bunit_gfx_semitrans_enabled,
                    g_bunit_text_digit_texture_page, clut, g_bunit_gfx_otag_index);
                negative = 0;
            } else {
                if (command->opcode == 0x1A) {
                    clut = g_bunit_text_metric_2;
                }
                if (blanks != 0) {
                    digit = 0;
                    blanks--;
                    if (command->opcode == 0x19) {
                        continue;
                    }
                    if (command->opcode == 0x1A && i < command->width - 1) {
                        clut = g_bunit_text_metric_3;
                    }
                } else {
                    digit = text[text_index] - '0';
                    text_index++;
                }
                if (command->style < 2) {
                    bunit_gfx_enqueue_textured_quad(&rect, glyph.x + glyph.w * digit, glyph.y, color,
                        g_bunit_gfx_semitrans_enabled, g_bunit_text_digit_texture_page, clut, g_bunit_gfx_otag_index);
                }
            }
        }
    }
    return (bunit_draw_number_command_t*)((u8*)command + command->length);
}

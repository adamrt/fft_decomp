#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/libc.h"
#include "psx/types.h"

/*
 * Render a formatted callback value with the menu's compact number glyphs.
 *
 * The command selects its value source, width, spacing, palette, and one of
 * three glyph layouts. High value bits request a disabled palette or dashes.
 */
world_draw_number_command_t* world_menu_script_draw_formatted_number(
    world_draw_number_command_t* command, s32 context) {
    s8 text[16];
    RECT rect;
    RECT glyph;
    RECT minus_rect;
    s32 (*getter)(s32, s32);
    s32 row;
    s32 scroll_index;
    u16 raw_step;
    s32 y;
    s32 value;
    s32 digit;
    s16 blanks;
    s32 text_index;
    s32 i;
    s32 negative;
    CVECTOR* color;
    u16 clut;
    /* Pin required: unpinned, the digit count loads into $s0 instead of $a2. */
    register s32 remaining __asm__("$6");
    s32 next_remaining;
    s32 x;
    s32 negative_state;

    getter = (s32 (*)(s32, s32))g_world_menu_script_callbacks[command->getter];
    if (g_world_menu_use_scroll_position == 0) {
        row = command->value;
        y = command->y;
    } else {
        row = g_world_menu_scroll_offset;
        scroll_index = g_world_menu_scroll_row_offset;
        row += scroll_index;
        raw_step = g_world_menu_row_height;
        /* Loads the halfword apart from the shifted context; otherwise the
         * getter pointer leaves $a2. */
        __asm__("" : "=r"(raw_step) : "0"(raw_step));
        context = raw_step << 16;
        y = (context >> 16) * scroll_index + command->y - g_world_menu_scroll_pixel_offset;
        if (g_world_menu_scroll_pixel_offset < 0) {
            row--;
            y -= context >> 16;
        }
    }
    negative = 0;
    if (getter != 0) {
        value = getter(row, context);
    } else {
        value = row;
    }
    if (value < 0) {
        value = -value;
        negative = 1;
    }
    {
        u8 opcode = command->opcode;

        if (opcode == 0x1B || (color = 0, opcode == 0x19)) {
            color = &g_world_menu_sprite_color;
        }
    }
    if (command->alternate_palette != 0) {
        clut = g_world_menu_alternate_number_clut;
    } else {
        clut = g_world_menu_palette_clut;
    }
    if (value & 0x40000000) {
        clut = g_world_menu_disabled_number_clut;
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
        remaining = command->width;
        if (remaining > 0) {
            do {
                x = command->x;
                next_remaining = remaining - 1;
                rect.x = x + next_remaining * command->spacing;
                rect.y = y + 5;
                rect.w = 6;
                rect.h = 3;
                world_gfx_enqueue_textured_quad(&rect, 0xBA, 0x13, color, g_world_menu_semi_trans,
                    g_world_menu_window_tpage, clut, g_world_menu_draw_priority);
                remaining = next_remaining;
            } while (remaining > 0);
        }
    } else {
        blanks = command->width - sprintf(text, "%d", value) - negative;
        text_index = 0;
        for (i = 0; i < command->width; i++) {
            rect.x = command->x + i * command->spacing;
            rect.y = y;
            rect.w = glyph.w;
            rect.h = glyph.h;
            negative_state = negative;
            /* The target copies the sign state before branching on it. */
            __asm__("" : "=r"(negative_state) : "0"(negative_state));
            if (negative_state != 0) {
                minus_rect.x = rect.x - 6;
                minus_rect.y = y + 2;
                minus_rect.w = 6;
                minus_rect.h = 0xB;
                if (command->opcode == 0x1A) {
                    minus_rect.x = 0xAE;
                }
                world_gfx_enqueue_textured_quad(&minus_rect, 0xBA, 0x10, color, g_world_menu_semi_trans,
                    g_world_menu_window_tpage, clut, g_world_menu_draw_priority);
                negative = 0;
            } else {
                if (command->opcode == 0x1A) {
                    clut = g_world_menu_palette_clut;
                }
                if (blanks != 0) {
                    digit = 0;
                    blanks--;
                    if (command->opcode == 0x19) {
                        continue;
                    }
                    if (command->opcode == 0x1A && i < command->width - 1) {
                        clut = g_world_menu_disabled_number_clut;
                    }
                } else {
                    digit = text[text_index] - '0';
                    text_index++;
                }
                if (command->style < 2) {
                    world_gfx_enqueue_textured_quad(&rect, glyph.x + glyph.w * digit, glyph.y, color,
                        g_world_menu_semi_trans, g_world_menu_window_tpage, clut, g_world_menu_draw_priority);
                }
            }
        }
    }
    return (world_draw_number_command_t*)((u8*)command + command->length);
}

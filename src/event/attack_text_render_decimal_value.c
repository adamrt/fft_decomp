#include "fft/attack.h"
#include "fft/battle_text.h"
#include "psx/types.h"

void attack_text_render_decimal_value(s32 value, s32 flags, void* pixels, status_panel_text_position_t* position) {
    s32 saved_color;
    s32 plus;
    s32 field_digits;
    /* Pin: with the clobber below, keeps the second `minimum_digits = 1` from folding into the first. */
    register s32 minimum_digits __asm__("$5");
    /* Pin: unpinned, GCC moves the minus test and the x offset out of $v0 into $a0. */
    register s32 work __asm__("$2");
    /* Pin: unpinned, GCC swaps $v1 and $a0 between the width test and field_digits. */
    register s32 width __asm__("$3");
    /* Pin: unpinned, GCC swaps $s2 and $s3 between digit_index and flags. */
    register s32 digit_index __asm__("$18");
    s32 quotient;
    /* Unused: the target reserves this frame space. */
    volatile u32 stack_padding[2];

    saved_color = position->color;
    position->color = 0;

    if ((flags & 0x0200) != 0) {
        if (value == 100) {
            flags = (flags & 0xff00) + 3;
        }
    }

    if ((flags & 0x8000) != 0) {
        position->x -= 2;
        g_attack_text_decimal_glyph.source_x = 0xb4;
        blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_attack_text_decimal_glyph, position);
        position->x += 7;
    }

    if ((flags & 0x0100) != 0) {
        g_attack_text_decimal_glyph.source_x = 0x78;
        g_attack_text_decimal_glyph.source_y = 0x1a;
        g_attack_text_decimal_glyph.height = 4;
        position->y += 3;
        blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_attack_text_decimal_glyph, position);
        position->x += 7;
        position->y -= 3;
        g_attack_text_decimal_glyph.source_y = 0x10;
        g_attack_text_decimal_glyph.height = 0x0a;
    }

    position->color = saved_color;

    if ((flags & MENU_DECIMAL_SIGN_MINUS) != 0) {
        g_attack_text_decimal_glyph.source_x = 0xba;
        if (value == 0) {
            if ((flags & MENU_DECIMAL_FIELD_WIDTH_MASK) == 4) {
                position->x += 10;
            } else {
                position->x += 5;
            }
        }
        blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_attack_text_decimal_glyph, position);
        position->x += 5;
        if (value == 0) {
            return;
        }
    }

    plus = flags & MENU_DECIMAL_SIGN_PLUS;
    if (plus != 0) {
        g_attack_text_decimal_glyph.source_x = 0xc8;
        blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_attack_text_decimal_glyph, position);
        position->x += 5;
    }

    field_digits = flags & MENU_DECIMAL_FIELD_WIDTH_MASK;
    if ((flags & 0x2000) != 0) {
        field_digits = 3;
    }
    if (plus != 0) {
        field_digits--;
    }
    work = flags & MENU_DECIMAL_SIGN_MINUS;
    minimum_digits = 1;
    if (work != 0) {
        field_digits--;
    }
    width = flags & MENU_DECIMAL_FIELD_WIDTH_MASK;
    if (value < 10) {
        /* Barrier: forgets that $5 already holds 1. */
        __asm__("" : : : "$5");
        minimum_digits = 1;
    }
    digit_index = 0;
    if (width != 1) {
        work = field_digits - minimum_digits;
        /* Barrier: keeps the offset in $v0 (`subu v0`) and the x load in $a0. */
        __asm__("" : : "r"(work));
        position->x += work * 5;
    }

    if ((flags & 0xff) > 0) {
        do {
            quotient = value / 10;
            g_attack_text_decimal_glyph.source_x = (value - quotient * 10) * 6 + 0x78;
            blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_attack_text_decimal_glyph, position);
            position->x -= 5;
            value = quotient;
            if ((flags & 0x4000) == 0 && value == 0) {
                break;
            }
            digit_index++;
        } while (digit_index < (flags & 0xff));
    }

    position->color = 0;
}

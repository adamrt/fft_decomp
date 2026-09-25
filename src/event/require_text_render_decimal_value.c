#include "fft/event_require.h"
#include "psx/types.h"

void require_text_render_decimal_value(
    s32 value, s32 flags, void* pixels, battle_menu_status_panel_text_position_t* position) {
    s32 saved_color;
    s32 plus;
    s32 digits;
    /* Pin: $a1, so the clobber below keeps the second `min_digits = 1`. */
    register s32 min_digits __asm__("$5");
    /* Pin: unpinned, i and flags swap $s2 and $s3. */
    register s32 i __asm__("$18");
    s32 quotient;
    /* Pin: with the use below, the offset is built in $v0 and the x load lands in $a0. */
    register s32 offset __asm__("$2");
    /* Unused: the target frame reserves 8 more bytes. */
    volatile u32 stack_padding[2];

    saved_color = position->color;
    position->color = 0;
    if ((flags & 0x0200) && value == 100) {
        flags = (flags & 0xff00) + 3;
    }
    if (flags & 0x8000) {
        position->x -= 2;
        g_require_text_decimal_glyph.source_x = 0xb4;
        blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_require_text_decimal_glyph, position);
        position->x += 7;
    }
    if (flags & 0x0100) {
        g_require_text_decimal_glyph.source_x = 0x78;
        g_require_text_decimal_glyph.source_y = 0x1a;
        g_require_text_decimal_glyph.height = 4;
        position->y += 3;
        blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_require_text_decimal_glyph, position);
        position->x += 7;
        position->y -= 3;
        g_require_text_decimal_glyph.source_y = 0x10;
        g_require_text_decimal_glyph.height = 0x0a;
    }
    position->color = saved_color;
    if (flags & MENU_DECIMAL_SIGN_MINUS) {
        g_require_text_decimal_glyph.source_x = 0xba;
        if (value == 0) {
            if ((flags & MENU_DECIMAL_FIELD_WIDTH_MASK) == 4) {
                position->x += 10;
            } else {
                position->x += 5;
            }
        }
        blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_require_text_decimal_glyph, position);
        position->x += 5;
        if (value == 0) {
            return;
        }
    }
    plus = flags & MENU_DECIMAL_SIGN_PLUS;
    if (plus) {
        g_require_text_decimal_glyph.source_x = 0xc8;
        blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_require_text_decimal_glyph, position);
        position->x += 5;
    }
    digits = flags & MENU_DECIMAL_FIELD_WIDTH_MASK;
    if (flags & 0x2000) {
        digits = 3;
    }
    if (plus) {
        digits--;
    }
    if (flags & MENU_DECIMAL_SIGN_MINUS) {
        digits--;
    }
    min_digits = 1;
    if (value < 10) {
        /* Clobbers $a1 so this `li a1,1` is kept, not folded into the first. */
        __asm__("" : : : "$5");
        min_digits = 1;
    }
    if ((flags & MENU_DECIMAL_FIELD_WIDTH_MASK) != 1) {
        offset = digits - min_digits;
        /* Keeps the offset in $v0 until the x update. */
        __asm__("" : : "r"(offset));
        position->x += offset * 5;
    }
    i = 0;
    if ((flags & 0xff) > 0) {
        do {
            quotient = value / 10;
            g_require_text_decimal_glyph.source_x = (value - quotient * 10) * 6 + 0x78;
            blit_text_glyph(g_battle_menu_glyph_image, pixels, &g_require_text_decimal_glyph, position);
            position->x -= 5;
            value = quotient;
            if (!(flags & 0x4000) && value == 0) {
                break;
            }
            i++;
        } while (i < (flags & 0xff));
    }
    position->color = 0;
}

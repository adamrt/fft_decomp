#include "fft/battle_text.h"
#include "fft/equip.h"
#include "psx/types.h"

void equip_text_render_decimal_value(
    s32 value, s32 flags, void* pixels, battle_menu_status_panel_text_position_t* position) {
    s32 current_value = value;
    s32 render_flags = flags;
    void* render_pixels = pixels;
    battle_menu_status_panel_text_position_t* render_position = position;
    s32 saved_color;
    /* Pins: unpinned, GCC swaps render_flags/digit_index ($s3/$s2) and glyph/pixels
     * ($s5/$s6) and folds the repeated `minimum_digits = 1`. */
    register s32 digit_index __asm__("$18");
    register battle_menu_status_panel_glyph_t* glyph __asm__("$21");
    register s32 updated_flags __asm__("$2");
    register s32 minimum_digits __asm__("$5");
    register s32 scratch __asm__("$3");
    volatile u32 stack_padding[2];
    s32 field_digits;
    s32 quotient;

    /* Two separate uses, in this order, save and copy pixels (s6) before position (s1). */
    __asm__("" : : "r"(render_pixels));
    __asm__("" : : "r"(render_position));

    saved_color = render_position->color;
    render_position->color = 0;

    if ((render_flags & 0x0200) != 0) {
        if (current_value == 100) {
            updated_flags = render_flags & 0xff00;
            render_flags = updated_flags + 3;
        }
    }

    if ((render_flags & 0x8000) != 0) {
        render_position->x -= 2;
        g_equip_text_decimal_glyph.source_x = 0xb4;
        blit_text_glyph(g_battle_menu_glyph_image, render_pixels, &g_equip_text_decimal_glyph, render_position);
        render_position->x += 7;
    }

    if ((render_flags & 0x0100) != 0) {
        g_equip_text_decimal_glyph.source_x = 0x78;
        g_equip_text_decimal_glyph.source_y = 0x1a;
        g_equip_text_decimal_glyph.height = 4;
        render_position->y += 3;
        blit_text_glyph(g_battle_menu_glyph_image, render_pixels, &g_equip_text_decimal_glyph, render_position);
        render_position->x += 7;
        render_position->y -= 3;
        g_equip_text_decimal_glyph.source_y = 0x10;
        g_equip_text_decimal_glyph.height = 0x0a;
    }

    render_position->color = saved_color;

    if ((render_flags & MENU_DECIMAL_SIGN_MINUS) != 0) {
        g_equip_text_decimal_glyph.source_x = 0xba;
        if (current_value == 0) {
            if ((render_flags & MENU_DECIMAL_FIELD_WIDTH_MASK) == 4) {
                render_position->x += 10;
            } else {
                render_position->x += 5;
            }
        }
        blit_text_glyph(g_battle_menu_glyph_image, render_pixels, &g_equip_text_decimal_glyph, render_position);
        render_position->x += 5;
        if (current_value == 0) {
            return;
        }
    }

    saved_color = render_flags & MENU_DECIMAL_SIGN_PLUS;
    if (saved_color != 0) {
        g_equip_text_decimal_glyph.source_x = 0xc8;
        blit_text_glyph(g_battle_menu_glyph_image, render_pixels, &g_equip_text_decimal_glyph, render_position);
        render_position->x += 5;
    }

    field_digits = render_flags & MENU_DECIMAL_FIELD_WIDTH_MASK;
    if ((render_flags & 0x2000) != 0) {
        field_digits = 3;
    }
    if (saved_color != 0) {
        field_digits--;
    }
    updated_flags = render_flags & MENU_DECIMAL_SIGN_MINUS;
    minimum_digits = 1;
    if (updated_flags != 0) {
        field_digits--;
    }

    scratch = render_flags & MENU_DECIMAL_FIELD_WIDTH_MASK;
    if (current_value < 10) {
        /* Clobbers $5 so this second `li a1,1` (and the test) is kept, not folded into the first. */
        __asm__("" : : : "$5");
        minimum_digits = 1;
    }
    digit_index = 0;
    if (scratch != 1) {
        updated_flags = field_digits - minimum_digits;
        /* Keeps the offset in v0 (`subu v0`) and the x load in a0 for the update below. */
        __asm__("" : : "r"(updated_flags));
        render_position->x += updated_flags * 5;
    }

    if ((render_flags & 0xff) > 0) {
        glyph = &g_equip_text_decimal_glyph;
        do {
            quotient = current_value / 10;
            scratch = quotient * 4;
            scratch = current_value - ((scratch + quotient) * 2);
            glyph->source_x = (scratch * 6) + 0x78;
            blit_text_glyph(g_battle_menu_glyph_image, render_pixels, glyph, render_position);
            render_position->x -= 5;
            current_value = quotient;
            if (((render_flags & 0x4000) != 0) || (current_value != 0)) {
                digit_index++;
                if (digit_index < (render_flags & 0xff)) {
                    continue;
                }
            }
            break;
        } while (1);
    }

    render_position->color = 0;
}

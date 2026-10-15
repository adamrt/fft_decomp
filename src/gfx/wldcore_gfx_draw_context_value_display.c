#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"

/* Draws the HUD numeric readout at the context-value display position, laid
 * out by g_wldcore_context_value_display_mode: 1 is the labeled, underlined
 * value (war funds), 2 the play time as hours, minutes and seconds with
 * separators, 0 and 3 a plain eight-digit value (3 under a small label
 * sprite) and 4 a labeled five-digit secondary value plus the mode-1 readout.
 *
 * Case 3 falls through into case 0, so the case bodies are ordered 1, 2, 3, 0,
 * 4 to follow the jump table at 0x80067020. The color triple stores into
 * `separator` precede those into `sprite`. The position read back at the end
 * of case 4 is a plain 32-bit load narrowed by the store, where the chained
 * assignment before the switch reads only the low 16 bits. */
void wldcore_gfx_draw_context_value_display(GsOT* ot) {
    GsSPRITE sprite;
    GsSPRITE separator;
    GsLINE line;
    CVECTOR color;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE) {
        return;
    }

    separator.attribute = sprite.attribute = 0;
    separator.tpage = sprite.tpage = GetTPage(0, 0, 0x3C0, 0x100);
    separator.cy = sprite.cy = 0x1E1;
    wldcore_gfx_copy_color_with_tint(&g_wldcore_hud_color, &color);
    separator.r = color.r;
    separator.g = color.g;
    separator.b = color.b;
    sprite.r = color.r;
    sprite.g = color.g;
    sprite.b = color.b;
    separator.x = sprite.x = g_wldcore_context_value_display_position.x;
    separator.y = sprite.y = g_wldcore_context_value_display_position.y;

    switch (g_wldcore_context_value_display_mode) {
    case WLDCORE_VALUE_DISPLAY_FUNDS:
        wldcore_gfx_draw_cursor_sprite_and_underline(ot, &sprite, &line);
        break;
    case WLDCORE_VALUE_DISPLAY_PLAY_TIME:
        sprite.u = 0x60;
        sprite.v = 0x10;
        sprite.w = 0x14;
        sprite.h = 8;
        sprite.cx = 0x20;
        GsSortFastSprite(&sprite, ot, g_wldcore_hud_ot_priority);

        separator.u = 0x28;
        separator.v = 0;
        separator.w = 6;
        separator.h = 8;
        separator.cx = 0x20;
        sprite.y += 8;
        separator.y = sprite.y + 4;
        sprite.x -= 8;
        wldcore_gfx_draw_number_digits(g_wldcore_displayed_numeric_value, 4, &sprite, ot);

        separator.x = sprite.x;
        GsSortFastSprite(&separator, ot, g_wldcore_hud_ot_priority);
        sprite.x += 6;
        wldcore_gfx_draw_number_digits_with_fill(g_wldcore_displayed_numeric_value_secondary, 2, &sprite, ot, 1);

        separator.x = sprite.x;
        GsSortFastSprite(&separator, ot, g_wldcore_hud_ot_priority);
        sprite.x += 6;
        wldcore_gfx_draw_number_digits_with_fill(g_wldcore_displayed_numeric_value_tertiary, 2, &sprite, ot, 1);
        break;
    case WLDCORE_VALUE_DISPLAY_LABELED:
        sprite.u = 0xA8;
        sprite.v = 0x20;
        sprite.w = 0xD;
        sprite.h = 9;
        sprite.cx = 0x20;
        GsSortFastSprite(&sprite, ot, g_wldcore_hud_ot_priority);
        /* fallthrough */
    case WLDCORE_VALUE_DISPLAY_PLAIN:
        sprite.x -= 0xC;
        sprite.y += 8;
        wldcore_gfx_draw_number_digits(g_wldcore_displayed_numeric_value, 8, &sprite, ot);
        break;
    case WLDCORE_VALUE_DISPLAY_COST_AND_FUNDS:
        sprite.u = 0x60;
        sprite.w = 0x18;
        sprite.h = 8;
        sprite.v = 0;
        sprite.cx = 0x20;
        sprite.x -= 0x40;
        GsSortFastSprite(&sprite, ot, g_wldcore_hud_ot_priority);

        sprite.x += 4;
        sprite.y += 8;
        wldcore_gfx_draw_number_digits_with_fill(g_wldcore_displayed_numeric_value_secondary, 5, &sprite, ot, 0);

        sprite.x = g_wldcore_context_value_display_position.x;
        sprite.y = g_wldcore_context_value_display_position.y;
        wldcore_gfx_draw_cursor_sprite_and_underline(ot, &sprite, &line);
        break;
    }
}

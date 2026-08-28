#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "psx/gs.h"
#include "psx/gte.h"
#include "psx/types.h"

u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);
void wldcore_gfx_copy_color_with_tint(CVECTOR* source, CVECTOR* out);
void wldcore_gfx_draw_number_digits(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot);
void wldcore_gfx_draw_number_digits_with_fill(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot, s32 zero_pad);
void world_gs_sortfastsprite(GsSPRITE* sprite, GsOT* ot, u16 pri);
void wldcore_gfx_draw_cursor_sprite_and_underline(GsOT* ot, GsSPRITE* sprite, GsLINE* line);

/* Draws the HUD numeric readout at the context-value display position, laid
 * out by g_wldcore_context_value_display_mode: 1 is the labelled, underlined
 * value (war funds), 2 the play time as hours, minutes and seconds with
 * separators, 0 and 3 a plain eight-digit value (3 under a small label
 * sprite) and 4 a labelled five-digit secondary value plus the mode-1 readout.
 *
 * Case 3 falls through into case 0, so the case bodies are ordered 1, 2, 3, 0,
 * 4 to follow the jump table at 0x80067020. The colour triple stores into
 * `separator` precede those into `sprite`. Case 4 updates sprite.x/sprite.y in
 * place (`sprite.x = sprite.x - 0x40`): a named local hoists the `lhu` above
 * the constant stores and rotates v0/v1. The two globals read back at the end
 * of case 4 need s32 locals because the target loads them with `lw` into u16
 * fields. */
void wldcore_gfx_draw_context_value_display(void* ot) {
    GsSPRITE sprite;
    GsSPRITE separator;
    GsLINE line;
    CVECTOR color;
    u16 tpage;

    if (g_main_system_flags & 0x800) {
        return;
    }

    sprite.attribute = 0;
    separator.attribute = 0;
    tpage = GetTPage(0, 0, 0x3C0, 0x100);
    sprite.tpage = tpage;
    separator.tpage = tpage;
    sprite.cy = 0x1E1;
    separator.cy = 0x1E1;
    wldcore_gfx_copy_color_with_tint(&g_wldcore_hud_color, &color);
    separator.r = color.r;
    separator.g = color.g;
    separator.b = color.b;
    sprite.r = color.r;
    sprite.g = color.g;
    sprite.b = color.b;
    sprite.x = g_wldcore_context_value_display_position.x;
    separator.x = g_wldcore_context_value_display_position.x;
    sprite.y = g_wldcore_context_value_display_position.y;
    separator.y = g_wldcore_context_value_display_position.y;

    switch (g_wldcore_context_value_display_mode) {
    case 1:
        wldcore_gfx_draw_cursor_sprite_and_underline(ot, &sprite, &line);
        break;
    case 2: {
        /* u16: the target reads GsSPRITE's short x/y with lhu. */
        u16 y;
        u16 x;

        sprite.u = 0x60;
        sprite.v = 0x10;
        sprite.w = 0x14;
        sprite.h = 8;
        sprite.cx = 0x20;
        world_gs_sortfastsprite(&sprite, ot, g_wldcore_hud_ot_priority);

        separator.u = 0x28;
        separator.v = 0;
        separator.w = 6;
        separator.h = 8;
        separator.cx = 0x20;
        y = sprite.y;
        sprite.y = y + 8;
        separator.y = y + 0xC;
        x = sprite.x;
        sprite.x = x - 8;
        wldcore_gfx_draw_number_digits(g_wldcore_displayed_numeric_value, 4, &sprite, ot);

        separator.x = sprite.x;
        world_gs_sortfastsprite(&separator, ot, g_wldcore_hud_ot_priority);
        sprite.x = sprite.x + 6;
        wldcore_gfx_draw_number_digits_with_fill(g_wldcore_displayed_numeric_value_secondary, 2, &sprite, ot, 1);

        separator.x = sprite.x;
        world_gs_sortfastsprite(&separator, ot, g_wldcore_hud_ot_priority);
        sprite.x = sprite.x + 6;
        wldcore_gfx_draw_number_digits_with_fill(g_wldcore_displayed_numeric_value_tertiary, 2, &sprite, ot, 1);
        break;
    }
    case 3:
        sprite.u = 0xA8;
        sprite.v = 0x20;
        sprite.w = 0xD;
        sprite.h = 9;
        sprite.cx = 0x20;
        world_gs_sortfastsprite(&sprite, ot, g_wldcore_hud_ot_priority);
        /* fallthrough */
    case 0: {
        u16 x;
        u16 y;

        x = sprite.x;
        y = sprite.y;
        sprite.x = x - 0xC;
        sprite.y = y + 8;
        wldcore_gfx_draw_number_digits(g_wldcore_displayed_numeric_value, 8, &sprite, ot);
        break;
    }
    case 4: {
        s32 display_x;
        s32 display_y;

        sprite.u = 0x60;
        sprite.w = 0x18;
        sprite.h = 8;
        sprite.v = 0;
        sprite.cx = 0x20;
        sprite.x = sprite.x - 0x40;
        world_gs_sortfastsprite(&sprite, ot, g_wldcore_hud_ot_priority);

        sprite.x = sprite.x + 4;
        sprite.y = sprite.y + 8;
        wldcore_gfx_draw_number_digits_with_fill(g_wldcore_displayed_numeric_value_secondary, 5, &sprite, ot, 0);

        display_x = g_wldcore_context_value_display_position.x;
        display_y = g_wldcore_context_value_display_position.y;
        sprite.x = display_x;
        sprite.y = display_y;
        wldcore_gfx_draw_cursor_sprite_and_underline(ot, &sprite, &line);
        break;
    }
    }
}

/* The target wraps `value / divisor` in ASPSX's zero and INT_MIN traps
 * (divcheck expansion). Splitting the division and the remainder into two
 * statements gives the target's order; `s32 saved_y` (not s16) makes the
 * saved y load `lh`. */

#include "fft/wldcore.h"
#include "psx/gs.h"
#include "psx/types.h"

void world_gs_sortfastsprite(GsSPRITE* sprite, GsOT* ot, u16 pri);

/* Draws value right-aligned as digits glyphs, one 8x12 or 8x15 cell per
 * digit, stepping the sprite eight pixels right per column. Leading zeros are
 * skipped: a column only draws once the running divisor has dropped to or
 * below value, and the last column always draws. */
void wldcore_gfx_draw_number_digits(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot) {
    s32 divisor;
    s32 i;
    s32 digit;
    s32 saved_y;

    divisor = 1;
    i = 0;
    sprite->w = 8;
    sprite->v = 0x31;
    saved_y = sprite->y;
    sprite->cx = 0x70;

    for (i = 0; i < digits - 1; i++) {
        divisor = divisor * 10;
    }

    for (i = 0; i < digits;) {
        if (value >= divisor || i == digits - 1) {
            digit = value / divisor;
            digit = digit % 10;
            sprite->u = digit * 8 - 0x58;
            if (digit < 6) {
                sprite->v = 0x34;
                sprite->h = 0xc;
                sprite->y = sprite->y + 4;
            } else {
                sprite->v = 0x31;
                sprite->h = 0xf;
                sprite->y = sprite->y + 1;
            }
            world_gs_sortfastsprite(sprite, ot, g_wldcore_hud_ot_priority);
            sprite->y = saved_y;
        }
        sprite->x = sprite->x + 8;
        divisor = divisor / 10;
        i++;
    }
}

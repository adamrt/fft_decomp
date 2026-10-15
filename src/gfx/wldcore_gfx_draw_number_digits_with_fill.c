#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Draws value right-aligned in `digits` columns like
 * wldcore_gfx_draw_number_digits, but also draws the columns ahead of the
 * first significant digit: a zero glyph when `zero_pad` is set, otherwise the
 * 8x8 filler glyph at (0xd0, 0x28). */
void wldcore_gfx_draw_number_digits_with_fill(s32 value, s32 digits, GsSPRITE* sprite, GsOT* ot, s32 zero_pad) {
    s32 divisor;
    s32 y;
    s32 digit;
    s32 i;

    divisor = 1;
    i = 0;
    sprite->w = 8;
    y = sprite->y;
    sprite->h = 0x10;
    for (; i < digits - 1; i++) {
        divisor *= 10;
    }
    for (i = 0; i < digits; i++) {
        if (value >= divisor || i == digits - 1) {
            digit = value / divisor;
            digit = digit % 10;
            sprite->cx = 0x70;
            sprite->u = digit * 8 - 0x58;
            if (digit < 6) {
                sprite->v = 0x34;
                sprite->h = 0xC;
                sprite->y += 4;
            } else {
                sprite->v = 0x31;
                sprite->h = 0xF;
                sprite->y += 1;
            }
            GsSortFastSprite(sprite, ot, g_wldcore_hud_ot_priority);
            sprite->y = y;
        } else {
            if (zero_pad != 0) {
                sprite->u = 0xA8;
                sprite->cx = 0x70;
                sprite->v = 0x31;
                sprite->h = 0xF;
                sprite->y += 1;
            } else {
                sprite->u = 0xD0;
                sprite->v = 0x28;
                sprite->cx = 0x70;
                sprite->h = 8;
                sprite->y += 4;
            }
            GsSortFastSprite(sprite, ot, g_wldcore_hud_ot_priority);
            sprite->y = y;
        }
        sprite->x += 8;
        divisor /= 10;
    }
}

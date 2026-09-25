#include "fft/wldcore.h"
#include "psx/gs.h"
#include "psx/types.h"

void world_gs_sortfastsprite(GsSPRITE* sprite, GsOT* ot, u16 pri);

/* Draws one cursor sprite plus its underline into the ordering table, then
 * hands the sprite to 0x8006bd84 for the trailing decoration.
 *
 * The line is derived from the sprite's current position; the sprite origin is
 * then advanced by (4, 8) so the follow-up draws at the shifted position. */
void wldcore_gfx_draw_cursor_sprite_and_underline(GsOT* ot, GsSPRITE* sprite, GsLINE* line) {
    /* u16: the target reads GsSPRITE's short x/y with lhu. */
    u16 x;
    u16 y;

    sprite->u = 0x9C;
    sprite->v = 0x78;
    sprite->w = 0x29;
    sprite->h = 8;
    sprite->cx = 0x20;
    world_gs_sortfastsprite(sprite, ot, g_wldcore_hud_ot_priority);

    line->attribute = 0;
    line->r = 0;
    line->g = 0;
    line->b = 0;
    x = sprite->x;
    line->x0 = x + 0x24;
    line->x1 = x + 0x28;
    y = sprite->y + 8;
    line->y1 = y;
    line->y0 = y;
    world_gs_sortline(line, ot, g_wldcore_hud_ot_priority);

    sprite->x += 4;
    sprite->y += 8;
    wldcore_gfx_draw_number_digits_with_fill(g_wldcore_displayed_numeric_value, 8, sprite, ot, 0);
}

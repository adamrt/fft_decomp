#include "fft/script.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draws the visible world-map routes into the auxiliary ordering table as
 * textured semi-transparent ribbons. Each route record is a run of packed
 * (x, y) pairs holding the two edges of the ribbon; every point is projected
 * through RotTransSV into one of four rotating vertex slots and flagged
 * in-bounds when x + 127 and y + 111 fall inside 255 x 227. Once four slots
 * are full they become one POLY_FT4 quad, emitted when either edge-leading
 * vertex was in bounds, and the last pair slides down to slots 0 and 1 so the
 * next pair closes the following segment. Gated as a whole by system flag
 * 0x20 and per route by script variable 556 + route.
 *
 * The count is sign-extended into count_tmp, which the loop guard tests before
 * the copy into count; the `j < count_tmp` guard leaves an unused compare
 * pseudo whose stack slot is the target's otherwise empty 32-bit stack slot at 0x88,
 * and taking `vertex` in the loop header numbers the vertex-base pseudo ahead
 * of it so the base spills to 0x80 as in the target. The 2-byte alignment of
 * SVECTOR makes `verts[0] = verts[2]` take GCC's unaligned block-move path
 * (lwl/lwr, swl/swr) as in the target. */
void wldcore_map_draw_visible_routes(GsOT* ot) {
    s32 flags[4];
    SVECTOR verts[4];
    SVECTOR in;
    POLY_FT4 poly;
    s32 projected;
    u16** table;
    u16* points;
    s32 count;
    s32 i;
    s32 j;
    s16 slot;
    SVECTOR* vertex;
    s16 raw;
    s32 count_tmp;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_HIDE_MAP_ROUTES) {
        return;
    }

    table = g_wldcore_map_route_polylines;
    SetPolyFT4(&poly);
    SetSemiTrans(&poly, 1);
    setUV4(&poly, 0x88, 0xE0, 0x97, 0xE0, 0x88, 0xEF, 0x97, 0xEF);
    setRGB0(
        &poly, g_wldcore_screen_brightness_rgb.r, g_wldcore_screen_brightness_rgb.g, g_wldcore_screen_brightness_rgb.b);
    poly.tpage = GetTPage(0, 0, 0x1C0, 0x100);
    poly.clut = GetClut(0x10, 0x1E8);

    for (i = 0, vertex = verts; i < WLDCORE_ROUTE_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_PATH_VISIBLE_FIRST) == 0) {
            continue;
        }
        slot = 0;
        points = table[i];
        raw = *points++;
        count_tmp = raw;
        in.vz = 0;
        j = 0;
        if (j < count_tmp) {
            count = count_tmp;
            do {
                in.vx = *points++;
                in.vy = *points++;
                RotTransSV(&in, &vertex[slot], &projected);
                if (vertex[slot].vx >= -127 && vertex[slot].vx <= 127 && vertex[slot].vy >= -111
                    && vertex[slot].vy <= 115) {
                    flags[slot] = 1;
                } else {
                    flags[slot] = 0;
                }
                slot++;
                if (slot == 4) {
                    poly.x0 = verts[0].vx;
                    poly.y0 = verts[0].vy;
                    poly.x1 = verts[1].vx;
                    poly.y1 = verts[1].vy;
                    poly.x2 = verts[2].vx;
                    slot -= 2;
                    poly.y2 = verts[2].vy;
                    poly.x3 = verts[3].vx;
                    poly.y3 = verts[3].vy;
                    verts[0] = verts[2];
                    verts[1] = verts[3];
                    if (flags[0] != 0 || flags[2] != 0) {
                        GsSortPoly(&poly, ot, 1);
                    }
                    flags[0] = flags[2];
                    flags[1] = flags[3];
                }
                j++;
            } while (j < count);
        }
    }
}

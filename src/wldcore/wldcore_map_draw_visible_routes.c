#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

#define WLDCORE_ROUTE_COUNT           48
#define SCRIPT_VAR_ROUTE_VISIBLE_BASE 556

/* Draws the visible world-map routes into the auxiliary ordering table as
 * textured semi-transparent ribbons. Each route record is a run of packed
 * (x, y) pairs holding the two edges of the ribbon; every point is projected
 * through func_8001D3E8 into one of four rotating vertex slots and flagged
 * in-bounds when x + 127 and y + 111 fall inside 255 x 227. Once four slots
 * are full they become one POLY_FT4 quad, emitted when either edge-leading
 * vertex was in bounds, and the last pair slides down to slots 0 and 1 so the
 * next pair closes the following segment. Gated as a whole by system flag
 * 0x20 and per route by script variable 556 + route.
 *
 * The count is sign-extended into count_tmp, which the loop guard tests before
 * the copy into count; the `j < count_tmp` guard leaves an unused compare
 * pseudo whose stack slot is the target's otherwise empty frame word at 0x88,
 * and taking `vertex` in the loop header numbers the vertex-base pseudo ahead
 * of it so the base spills to 0x80 as in the target. The 2-byte alignment of
 * SVECTOR makes `verts[0] = verts[2]` take GCC's unaligned block-move path
 * (lwl/lwr, swl/swr) as in the target. */
void wldcore_map_draw_visible_routes(void* ot) {
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
    u16 raw;
    s32 count_tmp;

    if (g_main_system_flags & 0x20) {
        return;
    }

    table = g_wldcore_map_route_polylines;
    SetPolyFT4(&poly);
    SetSemiTrans(&poly, 1);
    poly.u0 = 0x88;
    poly.v0 = 0xE0;
    poly.u1 = 0x97;
    poly.v1 = 0xE0;
    poly.u2 = 0x88;
    poly.v2 = 0xEF;
    poly.u3 = 0x97;
    poly.v3 = 0xEF;
    poly.r0 = g_wldcore_screen_brightness_rgb[0];
    poly.g0 = g_wldcore_screen_brightness_rgb[1];
    poly.b0 = g_wldcore_screen_brightness_rgb[2];
    poly.tpage = GetTPage(0, 0, 0x1C0, 0x100);
    poly.clut = GetClut(0x10, 0x1E8);

    for (i = 0, vertex = verts; i < WLDCORE_ROUTE_COUNT; i++) {
        if (world_script_get_variable(i + SCRIPT_VAR_ROUTE_VISIBLE_BASE) == 0) {
            continue;
        }
        slot = 0;
        points = table[i];
        raw = *points++;
        count_tmp = (s16)raw;
        in.vz = 0;
        j = 0;
        if (j < count_tmp) {
            count = count_tmp;
            do {
                in.vx = *points++;
                in.vy = *points++;
                RotTransSV(&in, &vertex[slot], &projected);
                if ((u16)(vertex[slot].vx + 127) < 255 && (u16)(vertex[slot].vy + 111) < 227) {
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
                        /* This caller sets only a0-a2, hence the three-argument cast. */
                        ((void (*)(POLY_FT4*, s32, s32))world_gs_sortpoly)(&poly, (s32)ot, 1);
                    }
                    flags[0] = flags[2];
                    flags[1] = flags[3];
                }
                j++;
            } while (j < count);
        }
    }
}

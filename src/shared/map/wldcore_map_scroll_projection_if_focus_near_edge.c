#include "fft/wldcore.h"
#include "psx/libgte.h"

/* Applies a scroll delta to the projection origin when the resulting window
 * origin would leave [-64, 72] horizontally or [-64, 64] vertically, and
 * raises system flag 0x2 if the origin's x or y changed. */
void wldcore_map_scroll_projection_if_focus_near_edge(point32_t* delta) {
    point32_t position;
    VECTOR saved;

    saved = g_wldcore_map_projection_origin;
    position.x = g_wldcore_map_projection_state.marker.render.position.x + delta->x;
    position.y = g_wldcore_map_projection_state.marker.render.position.y + delta->y;
    if ((delta->x < 0 && position.x < -0x40) || (delta->x > 0 && position.x >= 0x49)) {
        wldcore_map_apply_clamped_horizontal_scroll(&delta->x, &g_wldcore_map_projection_origin.vx);
    }
    if ((delta->y < 0 && position.y < -0x40) || (delta->y > 0 && position.y >= 0x41)) {
        wldcore_map_apply_clamped_vertical_scroll(delta, &g_wldcore_map_projection_origin);
    }
    if (saved.vx != g_wldcore_map_projection_origin.vx || saved.vy != g_wldcore_map_projection_origin.vy) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP;
    }
}

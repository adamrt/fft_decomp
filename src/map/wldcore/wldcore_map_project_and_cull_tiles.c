#include "fft/wldcore.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Builds the map camera matrix and projects the 13x17 grid of tile corners,
 * storing each screen position and setting flag 0x100 on corners that fall
 * outside the visible window; empty tiles (flag 0x200) keep their flags. */
void wldcore_map_project_and_cull_tiles(void) {
    MATRIX matrix;
    SVECTOR position;
    VECTOR projected;
    long flag;
    s32 i;
    wldcore_projected_entry_t* entry;
    wldcore_map_projection_view_t* view;
    SVECTOR* rotation;
    s32 flags;
    s32 updated;
    s32 coordinate;

    /* Retail keeps the camera base at rotation; the record base changes its argument instructions. */
    rotation = &g_wldcore_map_projection_rotation;
    view = (wldcore_map_projection_view_t*)((u8*)rotation - offsetof(wldcore_map_projection_view_t, rotation));
    RotMatrix(rotation, &matrix);
    TransMatrix(&matrix, &view->origin);
    ScaleMatrix(&matrix, &view->scale);
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);

    i = 0;
    entry = g_wldcore_map_projected_tiles;
    for (; i < 221; i++) {
        coordinate = g_wldcore_map_projected_tiles[i].map_x;
        position.vx = coordinate;
        coordinate = g_wldcore_map_projected_tiles[i].map_y;
        position.vz = 0;
        position.vy = coordinate;
        RotTrans(&position, &projected, &flag);
        g_wldcore_map_projected_tiles[i].screen_x = projected.vx;
        g_wldcore_map_projected_tiles[i].screen_y = projected.vy;
        flags = entry->flags;
        if (!(flags & WLDCORE_MAP_TILE_EMPTY)) {
            if (projected.vx < -0xa0 || projected.vx > 0x80 || projected.vy < -0x98 || projected.vy > 0x78) {
                updated = flags | WLDCORE_MAP_TILE_OFF_SCREEN;
            } else {
                updated = flags & ~WLDCORE_MAP_TILE_OFF_SCREEN;
            }
            entry->flags = updated;
        }
        entry++;
    }
}

#include "fft/wldcore.h"
#include "psx/gte.h"
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
    u8* rotation;
    s32 flags;
    s32 updated;
    s32 coordinate;

    rotation = g_wldcore_map_projection_rotation_bytes;
    RotMatrix(rotation, &matrix);
    TransMatrix(&matrix, rotation - 0x20);
    ScaleMatrix(&matrix, rotation - 0x10);
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
        if (!(flags & 0x200)) {
            if ((u32)(projected.vx + 0xa0) >= 0x121 || (u32)(projected.vy + 0x98) >= 0x111) {
                updated = flags | 0x100;
            } else {
                updated = flags & ~0x100;
            }
            entry->flags = updated;
        }
        entry++;
    }
}

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Places the camera for a map's GNS camera-position record.
 *
 * The position is the negated coords in 20.12 and the angles are masked to
 * 0x1fff. The cursor goes to the tile under the position (0x1c000 per tile) on
 * layer 0, and the camera height to that tile's height, 0xc000 (12 world units)
 * per level, negative upwards. */
void camera_set_focus_and_cursor_tile(const SVECTOR* coords, const SVECTOR* rotation) {
    s32 tile_x;
    s32 tile_y;

    main_util_set_vector(&g_camera_current_real_coords, -coords->vx * ONE, -coords->vy * ONE, -coords->vz * ONE);
    main_util_set_svector(&g_camera_render_state, rotation->vx & 0x1fff, rotation->vy & 0x1fff, rotation->vz & 0x1fff);
    g_battle_cursor_z = 0;
    tile_x = g_camera_current_real_coords.vx / (MAP_TILE_SIZE * ONE);
    g_battle_cursor_x = tile_x;
    tile_y = g_camera_current_real_coords.vz / (MAP_TILE_SIZE * ONE);
    g_battle_cursor_y = tile_y;
    g_camera_current_real_coords.vy = -map_get_tile_data_pointer(tile_x, tile_y, 0)->height * (MAP_HEIGHT_STEP * ONE);
}

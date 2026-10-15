#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "psx/types.h"

/* Aim the camera vector from the current camera focus at the cursor tile.
 *
 * The tile surface height is (height + depth) * 12 + half-height * 6 world
 * units above the floor, negated because world Y points down. The focus
 * offset is normalized by VectorNormal into g_camera_step_vector and
 * tripled. Returns the negated tile height. */
s32 camera_calculate_cursor_tile_vector_normal(void) {
    map_tile_t* tile;
    SVECTOR tile_position;
    VECTOR delta;
    s32 height;
    map_tile_depth_t depth;

    tile = map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1);
    depth = tile->depth_half_height;
    height = -((tile->height + depth.depth) * MAP_HEIGHT_STEP + depth.half_height * (MAP_HEIGHT_STEP / 2));
    main_util_set_svector(&tile_position, g_battle_cursor_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), height,
        g_battle_cursor_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
    setVector(&delta, tile_position.vx - g_camera_current_real_coords.vx / ONE,
        height - g_camera_current_real_coords.vy / ONE, tile_position.vz - g_camera_current_real_coords.vz / ONE);
    VectorNormal(&delta, &g_camera_step_vector);
    g_camera_step_vector.vx *= 3;
    g_camera_step_vector.vy *= 3;
    g_camera_step_vector.vz *= 3;
    return height;
}

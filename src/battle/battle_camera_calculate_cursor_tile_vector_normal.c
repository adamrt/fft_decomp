#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "psx/types.h"

/* Aim the camera vector from the current camera focus at the cursor tile.
 *
 * The tile surface height is (height + depth) * 12 + half-height * 6 world
 * units above the floor, negated because world Y points down. The focus
 * offset is transformed by func_8001C068 into g_battle_current_vector and
 * tripled. Returns the negated tile height. */
s32 battle_camera_calculate_cursor_tile_vector_normal(void) {
    map_tile_t* tile;
    SVECTOR tile_position;
    VECTOR delta;
    s32 height;
    u8 depth;

    tile = battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1);
    depth = tile->depth_half_height;
    height = -((tile->height + (depth >> MAP_TILE_DEPTH_SHIFT)) * 12 + (depth & MAP_TILE_HALF_HEIGHT_MASK) * 6);
    main_util_set_svector(&tile_position, g_battle_cursor_x * 28 + 14, height, g_battle_cursor_y * 28 + 14);
    delta.vx = tile_position.vx - g_battle_camera_current_real_coords.vx / ONE;
    delta.vy = height - g_battle_camera_current_real_coords.vy / ONE;
    delta.vz = tile_position.vz - g_battle_camera_current_real_coords.vz / ONE;
    VectorNormal(&delta, &g_battle_current_vector);
    g_battle_current_vector.vx *= 3;
    g_battle_current_vector.vy *= 3;
    g_battle_current_vector.vz *= 3;
    return height;
}

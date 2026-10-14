#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/* Project the cursor tile to the screen.
 *
 * The tile center (x * 28 + 14, surface height, y * 28 + 14) is raised by the
 * tile's depth, offset by the modify-by-5 or -by-1 conversion depending on
 * surface bit 0x40, and transformed by the battle camera. The projected X and
 * Y are kept in g_battle_cursor_screen_x/g_battle_cursor_screen_y for camera_update_cursor_tile_vector. */
void target_project_cursor_tile_to_screen(VECTOR* projected, SVECTOR* position, SVECTOR* raised) {
    map_tile_t* tile;
    long flag;

    battle_gfx_update_unit_palette_animation();
    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);
    main_util_set_svector(position, g_battle_cursor_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), 0,
        g_battle_cursor_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
    position->vy = map_calculate_slope_height(position, (u8)g_battle_cursor_z);
    tile = map_get_tile_data_pointer((position->vx / MAP_TILE_SIZE), (position->vz / MAP_TILE_SIZE), g_battle_cursor_z);
    position->vy -= tile->depth_half_height.depth * 12;
    if (tile->surface.depth_bias) {
        camera_convert_screen_coords_modify_by_5(position, raised);
    } else {
        camera_convert_screen_coords_modify_by_1(position, raised);
    }
    RotTrans(raised, projected, &flag);
    g_battle_cursor_screen_x = projected->vx;
    g_battle_cursor_screen_y = projected->vy;
}

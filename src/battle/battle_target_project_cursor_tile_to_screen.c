#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_gfx.h"
#include "psx/types.h"

/* Project the cursor tile to the screen.
 *
 * The tile centre (x * 28 + 14, surface height, y * 28 + 14) is raised by the
 * tile's depth, offset by the modify-by-5 or -by-1 conversion depending on
 * surface bit 0x40, and transformed by the battle camera. The projected X and
 * Y are kept in g_battle_cursor_screen_x/g_battle_cursor_screen_y for battle_camera_update_cursor_tile_vector. */
void battle_target_project_cursor_tile_to_screen(VECTOR* projected, SVECTOR* position, SVECTOR* raised) {
    map_tile_t* tile;
    long flag;

    battle_gfx_update_unit_palette_animation();
    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);
    main_util_set_svector(position, g_battle_cursor_x * 28 + 14, 0, g_battle_cursor_y * 28 + 14);
    position->vy = battle_map_calculate_slope_height(&position->vx, g_battle_cursor_z);
    tile = battle_map_get_tile_data_pointer((s16)(position->vx / 28), (s16)(position->vz / 28), g_battle_cursor_z);
    position->vy -= (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT) * 12;
    if (tile->surface.value & MAP_TILE_SURFACE_FLAG_6) {
        battle_camera_convert_screen_coords_modify_by_5(&position->vx, &raised->vx);
    } else {
        battle_camera_convert_screen_coords_modify_by_1(&position->vx, &raised->vx);
    }
    RotTrans(raised, projected, &flag);
    g_battle_cursor_screen_x = projected->vx;
    g_battle_cursor_screen_y = projected->vy;
}

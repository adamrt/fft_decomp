#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_move.h"
#include "fft/map.h"
#include "psx/gte.h"

/* Projects every display unit into camera space.
 *
 * Refreshes each item-effect origin and ordering-table depth. Terrain depth
 * lowers grounded units, while In Water, Float, and Walk on Water select
 * their original exceptions. */
void battle_gfx_update_all_unit_rotation_and_vectors(void) {
    VECTOR transformed;
    SVECTOR screen;
    SVECTOR adjusted;
    long gte_flag;
    battle_unit_misc_data_t* unit;
    map_tile_t* tile;
    battle_move_effective_flags_e movement_flags;

    SetRotMatrix(&g_battle_camera_matrix);
    SetTransMatrix(&g_battle_camera_matrix);

    unit = g_battle_misc_unit_list_head;
    while (unit != 0) {
        battle_unit_copy_misc_data_xyz_values(&screen, unit);
        RotTrans(&screen, &transformed, &unit->status_bubble_gte_flag);

        if ((unit->sprite_display_flags.half & BATTLE_MISC_DISPLAY_HORIZONTAL_FLIP) != 0) {
            unit->item_get_camera_x = (u16)transformed.vx - unit->screen_offset.vx;
        } else {
            unit->item_get_camera_x = (u16)transformed.vx + unit->screen_offset.vx;
        }
        if ((unit->sprite_display_flags.half & BATTLE_MISC_DISPLAY_VERTICAL_FLIP) != 0) {
            unit->item_get_camera_y = (u16)transformed.vy - unit->screen_offset.vy;
        } else {
            unit->item_get_camera_y = (u16)transformed.vy + unit->screen_offset.vy;
        }

        tile = battle_map_get_tile_data_pointer(unit->screen.vx / 28, unit->screen.vz / 28, unit->map_z);
        if (unit->centre_tile_offset != 0) {
            if ((tile->surface.value & MAP_TILE_SURFACE_FLAG_6) != 0) {
                battle_camera_calculate_relative_offset_5(&screen.vx, &adjusted.vx);
            } else {
                battle_camera_calculate_relative_offset_1(&screen.vx, &adjusted.vx);
            }
        } else if ((tile->surface.value & MAP_TILE_SURFACE_FLAG_6) != 0) {
            battle_camera_convert_screen_coords_modify_by_5(&screen.vx, &adjusted.vx);
        } else {
            battle_camera_convert_screen_coords_modify_by_1(&screen.vx, &adjusted.vx);
        }

        if (unit->battle_data != 0) {
            movement_flags = battle_move_get_effective_flags(unit->battle_data);
            if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) != 0) {
                if ((tile->depth_half_height & MAP_TILE_DEPTH_MASK) != 0) {
                    adjusted.vy -= 12;
                }
            } else if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT) == 0) {
                if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER) == 0) {
                    adjusted.vy -= (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT) * 12;
                }
            }
        }

        RotTrans(&adjusted, &transformed, &gte_flag);
        unit->otag_depth_index = transformed.vz / 4;
        unit = unit->previous;
    }
}

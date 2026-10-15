#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "psx/libgte.h"

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
    battle_unit_t* unit;
    map_tile_t* tile;
    move_effective_flags_e movement_flags;

    SetRotMatrix(&g_camera_matrix);
    SetTransMatrix(&g_camera_matrix);

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        battle_unit_copy_draw_coords(&screen, unit);
        RotTrans(&screen, &transformed, &unit->gte_flag);

        if ((unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) != 0) {
            unit->camera_relative_x = transformed.vx - unit->screen_offset.vx;
        } else {
            unit->camera_relative_x = transformed.vx + unit->screen_offset.vx;
        }
        if ((unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP) != 0) {
            unit->camera_relative_y = transformed.vy - unit->screen_offset.vy;
        } else {
            unit->camera_relative_y = transformed.vy + unit->screen_offset.vy;
        }

        tile = map_get_tile_data_pointer(
            unit->world_coords.vx / MAP_TILE_SIZE, unit->world_coords.vz / MAP_TILE_SIZE, unit->map_z);
        if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
            if ((tile->surface.depth_bias) != 0) {
                camera_calculate_relative_offset_by_5(&screen, &adjusted);
            } else {
                camera_calculate_relative_offset_by_1(&screen, &adjusted);
            }
        } else if ((tile->surface.depth_bias) != 0) {
            camera_convert_screen_coords_modify_by_5(&screen, &adjusted);
        } else {
            camera_convert_screen_coords_modify_by_1(&screen, &adjusted);
        }

        if (unit->stats != 0) {
            movement_flags = move_get_effective_flags(unit->stats);
            if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) != 0) {
                if (tile->depth_half_height.depth != 0) {
                    adjusted.vy -= 12;
                }
            } else if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT) == 0) {
                if ((movement_flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER) == 0) {
                    adjusted.vy -= tile->depth_half_height.depth * 12;
                }
            }
        }

        RotTrans(&adjusted, &transformed, &gte_flag);
        unit->otag_depth_index = transformed.vz / 4;
        unit = unit->previous;
    }
}

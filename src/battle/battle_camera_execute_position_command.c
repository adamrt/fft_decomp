#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

/* Timeline track 2 handler: sets the effect camera position target and duration.
 *
 * Mode bits 0x1e0 pick the source: effect target `target` (0x0, which also
 * saves the current camera position to g_battle_effect_camera_position_start), the given vector (0x40),
 * the map centre (0x80), the current camera (0xc0), the saved position at
 * g_battle_effect_camera_position_saved (0x100), the caster entry (0x140), the average of all unit
 * targets (0x180) or the tile at g_battle_effect_target_tile (0x1c0). All but 0x40 add offset
 * when present. Bits 0x1e00 are kept in g_battle_effect_camera_position_mode. */
void battle_camera_execute_position_command(s32 flags, s32 target, s32 duration, VECTOR* offset) {
    VECTOR sum;
    VECTOR* coords;
    battle_screen_coords_t* screen;
    s32 i;

    switch (flags & 0x1e0) {
    case 0x0:
        if (g_battle_effect_targets[target].target_type == 0) {
            battle_effect_copy_misc_unit_screen_location(
                g_battle_effect_targets[target].id.misc_id, &g_battle_effect_camera_position_target);
        } else {
            battle_effect_convert_tile_coords_to_world_coords(
                &g_battle_effect_targets[target].id.tile_x, &g_battle_effect_camera_position_target.vx);
        }
        coords = battle_camera_get_current_real_coords();
        battle_effect_shift_vector_right_12(&coords->vx, &g_battle_effect_camera_position_start.vx);
        if (offset) {
            battle_effect_add_vectors(
                &offset->vx, &g_battle_effect_camera_position_target.vx, &g_battle_effect_camera_position_target.vx);
        }
        break;
    case 0x40:
        g_battle_effect_camera_position_target = *offset;
        break;
    case 0x80:
        battle_effect_store_map_center_coordinates(&g_battle_effect_camera_position_target);
        if (offset) {
            battle_effect_add_vectors(
                &offset->vx, &g_battle_effect_camera_position_target.vx, &g_battle_effect_camera_position_target.vx);
        }
        break;
    case 0xc0:
        coords = battle_camera_get_current_real_coords();
        g_battle_effect_camera_position_target.vx = coords->vx >> 12;
        g_battle_effect_camera_position_target.vy = coords->vy >> 12;
        g_battle_effect_camera_position_target.vz = coords->vz >> 12;
        if (offset) {
            battle_effect_add_vectors(
                &offset->vx, &g_battle_effect_camera_position_target.vx, &g_battle_effect_camera_position_target.vx);
        }
        break;
    case 0x100:
        g_battle_effect_camera_position_target = g_battle_effect_camera_position_saved;
        if (offset) {
            battle_effect_add_vectors(
                &offset->vx, &g_battle_effect_camera_position_target.vx, &g_battle_effect_camera_position_target.vx);
        }
        break;
    case 0x140:
        if (g_battle_effect_targets[16].target_type == 0) {
            battle_effect_copy_misc_unit_screen_location(
                g_battle_effect_targets[16].id.misc_id, &g_battle_effect_camera_position_target);
        } else {
            battle_effect_convert_tile_coords_to_world_coords(
                &g_battle_effect_targets[16].id.tile_x, &g_battle_effect_camera_position_target.vx);
        }
        if (offset) {
            battle_effect_add_vectors(
                &offset->vx, &g_battle_effect_camera_position_target.vx, &g_battle_effect_camera_position_target.vx);
        }
        break;
    case 0x180:
        sum.vx = 0;
        sum.vy = 0;
        sum.vz = 0;
        for (i = 0; i < g_battle_effect_coord_data.hit_counter; i++) {
            if (g_battle_effect_targets[i].target_type == 0) {
                screen = battle_unit_get_screen_data_ptr_by_misc_id(g_battle_effect_targets[i].id.misc_id);
                sum.vx += screen->x;
                sum.vy += screen->z;
                sum.vz += screen->y;
            }
        }
        sum.vx /= i;
        sum.vy /= i;
        sum.vz /= i;
        if (offset) {
            battle_effect_add_vectors(&offset->vx, &sum.vx, &g_battle_effect_camera_position_target.vx);
        }
        break;
    case 0x1c0:
        battle_effect_convert_tile_coords_to_world_coords(
            &g_battle_effect_target_tile.id.tile_x, &g_battle_effect_camera_position_target.vx);
        if (offset) {
            battle_effect_add_vectors(
                &offset->vx, &g_battle_effect_camera_position_target.vx, &g_battle_effect_camera_position_target.vx);
        }
        break;
    default:
        return;
    }
    g_battle_effect_camera_position_mode = flags & 0x1e00;
    g_battle_effect_camera_position_duration = duration;
    g_battle_effect_camera_position_frame = 0;
}

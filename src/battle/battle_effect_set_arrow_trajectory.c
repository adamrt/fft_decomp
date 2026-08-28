#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Sets an arrow's source, destination and flight duration.
 *
 * The projectile origin is the first unit raised by two thirds of its
 * sprite height, and the destination is the second unit raised by three
 * quarters of its height, or, when target_id is -1, to the centre of the tile
 * at tile_position (x, layer, y) on its surface. g_battle_effect_trajectory_step_count receives the
 * flight duration derived from the distance, as in
 * battle_effect_check_direct_trajectory_between_units / battle_effect_trace_projectile_path.
 * `delta` reproduces the target's stores to an otherwise unused stack vector.
 */
void battle_effect_set_arrow_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id) {
    VECTOR delta;
    map_tile_t* tile;
    s32 source_height;
    s32 target_height;

    source_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    if (target_id != -1) {
        target_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id) * 3 / 4;
    }
    battle_unit_get_tile_center_and_height_by_battle_id(
        source_id, (battle_screen_coords_t*)&g_battle_effect_trajectory_origin);
    g_battle_effect_trajectory_origin.vy -= source_height;
    if (target_id == -1) {
        tile = battle_map_get_tile_data_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_battle_effect_trajectory_destination.vx = tile_position->vx * 28 + 14;
        g_battle_effect_trajectory_destination.vz = tile_position->vz * 28 + 14;
        g_battle_effect_trajectory_destination.vy = -(tile->height * 12);
    } else {
        battle_unit_get_tile_center_and_height_by_battle_id(
            target_id, (battle_screen_coords_t*)&g_battle_effect_trajectory_destination);
        g_battle_effect_trajectory_destination.vy -= target_height;
    }
    delta.vx = g_battle_effect_trajectory_destination.vx - g_battle_effect_trajectory_origin.vx;
    delta.vy = g_battle_effect_trajectory_destination.vy - g_battle_effect_trajectory_origin.vy;
    delta.vz = g_battle_effect_trajectory_destination.vz - g_battle_effect_trajectory_origin.vz;
    g_battle_effect_trajectory_step_count
        = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) << 12) / 8 >> 12;
}

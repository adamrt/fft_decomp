#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Sets an arrow's source, destination and flight duration.
 *
 * The projectile origin is the first unit raised by two thirds of its
 * sprite height, and the destination is the second unit raised by three
 * quarters of its height, or, when target_id is -1, to the center of the tile
 * at tile_position (x, layer, y) on its surface. g_effect_trajectory_step_count receives the
 * flight duration derived from the distance, as in
 * effect_check_direct_trajectory_between_units / effect_trace_projectile_path.
 * `delta` reproduces the target's stores to an otherwise unused stack vector.
 */
void effect_set_arrow_trajectory(s32 source_id, SVECTOR* tile_position, s32 target_id) {
    VECTOR delta;
    map_tile_t* tile;
    s32 source_height;
    s32 target_height;

    source_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    if (target_id != -1) {
        target_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id) * 3 / 4;
    }
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &g_effect_trajectory_origin);
    g_effect_trajectory_origin.vy -= source_height;
    if (target_id == -1) {
        tile = map_get_tile_data_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_effect_trajectory_destination.vx = tile_position->vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_effect_trajectory_destination.vz = tile_position->vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_effect_trajectory_destination.vy = -(tile->height * 12);
    } else {
        battle_unit_get_tile_center_and_height_by_battle_id(target_id, &g_effect_trajectory_destination);
        g_effect_trajectory_destination.vy -= target_height;
    }
    setVector(&delta, g_effect_trajectory_destination.vx - g_effect_trajectory_origin.vx,
        g_effect_trajectory_destination.vy - g_effect_trajectory_origin.vy,
        g_effect_trajectory_destination.vz - g_effect_trajectory_origin.vz);
    g_effect_trajectory_step_count
        = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE) / 8 >> 12;
}

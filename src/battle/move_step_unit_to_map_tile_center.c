#include "fft/battle_map.h"
#include "fft/battle_move.h"

/* Moves a unit to the center of its map tile over distortion_timer frames.
 *
 * Phase 0 splits the remaining screen-to-tile distance into equal Q12 steps;
 * phase 1 applies them (height only when allow_height_change is set) and
 * snaps to the tile once the timer runs out. */
void move_step_unit_to_map_tile_center(battle_unit_t* unit, s32 allow_height_change) {
    SVECTOR tile_center;
    VECTOR velocity;
    s32 timer;
    s32 target_z;

    timer = unit->distortion_timer;
    target_z = unit->distortion_target;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        velocity.vx = (unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2) - unit->world_coords.vx) * ONE;
        velocity.vz = (unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2) - unit->world_coords.vz) * ONE;
        tile_center.vx = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        tile_center.vz = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        velocity.vy = (map_calculate_slope_height(&tile_center, unit->map_z) - unit->world_coords.vy) * ONE;
        velocity.vx /= timer;
        velocity.vy /= timer;
        velocity.vz /= timer;
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            if (allow_height_change) {
                unit->real.vy += velocity.vy;
            }
            unit->real.vz += velocity.vz;
        } else {
            battle_unit_set_real_coords_from_map_coords(unit);
            unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    battle_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
    unit->distortion_target = target_z;
}

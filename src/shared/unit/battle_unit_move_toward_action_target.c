#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"

/* Moves a unit toward its action target over distortion_timer frames.
 *
 * Phase 0 picks the destination and splits the remaining distance into equal
 * Q12 steps: during an event the unit's own map tile, else the first entry of
 * the target list, else the recorded action target panel.  Phase 1 applies the
 * steps (height only when allow_height_change is set). */
void battle_unit_move_toward_action_target(battle_unit_t* unit, s32 allow_height_change) {
    SVECTOR tile_center;
    VECTOR velocity;
    s32 timer;
    s32 target_z;
    battle_unit_t* target;

    timer = unit->distortion_timer;
    target_z = unit->distortion_target;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->stats != 0) {
            if (g_battle_game_state == BATTLE_GAME_STATE_EVENT) {
                velocity.vx = ((unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)) * ONE) - unit->real.vx;
                velocity.vz = ((unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)) * ONE) - unit->real.vz;
                velocity.vy = 0;
            } else if (unit->strike.target_count != 0) {
                target = battle_unit_get_by_battle_id(unit->strike.target_list[0]);
                setVector(&velocity, target->real.vx - unit->real.vx, target->real.vy - unit->real.vy,
                    target->real.vz - unit->real.vz);
            } else {
                velocity.vx = (unit->stats->action_command.target_position.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)
                                  - unit->world_coords.vx)
                    * ONE;
                velocity.vz = (unit->stats->action_command.target_position.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)
                                  - unit->world_coords.vz)
                    * ONE;
                tile_center.vx = unit->stats->action_command.target_position.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                tile_center.vz = unit->stats->action_command.target_position.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                velocity.vy = (map_calculate_slope_height(&tile_center, unit->stats->action_command.target_position.vy)
                                  - unit->world_coords.vy)
                    * ONE;
            }
            velocity.vx /= timer;
            velocity.vy /= timer;
            velocity.vz /= timer;
            unit->distortion_phase++;
        } else {
            unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
            break;
        }
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            if (allow_height_change) {
                unit->real.vy += velocity.vy;
            }
            unit->real.vz += velocity.vz;
        } else {
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

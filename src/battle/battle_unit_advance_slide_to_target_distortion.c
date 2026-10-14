#include "fft/battle_unit.h"
#include "fft/map.h"

/* Distortion animation 0x0f: slide a unit away from its target over
 * distortion_timer frames (Rush winds up with it).
 *
 * Phase 0 aims at the first unit in the target list, or at the center of the
 * action target tile (28 screen units per tile), reduces each horizontal step
 * by an eighth, wraps it into one 0xe000 span, and divides it into per-frame
 * velocity. Phase 1 applies that velocity until the timer runs out. Clearing
 * velocity.vy in each branch keeps its division, which GCC folds away when the
 * zero is stored once after the branches. `unused_10` reproduces an
 * unreferenced 8-byte frame slot below the velocity copy. */
void battle_unit_advance_slide_to_target_distortion(battle_unit_t* unit) {
    s32 unused_10[2];
    VECTOR velocity;
    battle_unit_t* target;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->stats != 0) {
            if (unit->strike.target_count != 0) {
                target = battle_unit_get_by_battle_id(unit->strike.target_list[0]);
                velocity.vx = unit->real.vx - target->real.vx;
                velocity.vx = (velocity.vx - velocity.vx / 8) % 0xe000;
                velocity.vz = unit->real.vz - target->real.vz;
                velocity.vz = (velocity.vz - velocity.vz / 8) % 0xe000;
                velocity.vy = 0;
            } else {
                velocity.vx
                    = (unit->world_coords.vx
                          - (unit->stats->action_command.target_position.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)))
                    * ONE;
                velocity.vx = (velocity.vx - velocity.vx / 8) % 0xe000;
                velocity.vz
                    = (unit->world_coords.vz
                          - (unit->stats->action_command.target_position.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2)))
                    * ONE;
                velocity.vz = (velocity.vz - velocity.vz / 8) % 0xe000;
                velocity.vy = 0;
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
}

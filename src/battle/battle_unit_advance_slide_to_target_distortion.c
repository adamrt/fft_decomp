#include "fft/battle.h"

/* Distortion animation 0x0f: slide a unit toward its target over
 * distortion_timer frames (used by Rush).
 *
 * Phase 0 aims at the first unit in the target list, or at the centre of the
 * action target tile (28 screen units per tile), reduces each horizontal step
 * by an eighth, wraps it into one 0xe000 span, and divides it into per-frame
 * velocity. Phase 1 applies that velocity until the timer runs out. Clearing
 * velocity.vy in each branch keeps its division, which GCC folds away when the
 * zero is stored once after the branches. `unused_10` reproduces an
 * unreferenced 8-byte frame slot below the velocity copy. */
void battle_unit_advance_slide_to_target_distortion(battle_unit_misc_data_t* unit) {
    s32 unused_10[2];
    VECTOR velocity;
    battle_unit_misc_data_t* target;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->battle_data != 0) {
            if (unit->target_count != 0) {
                target = battle_unit_get_misc_data_by_battle_id(unit->target_list[0]);
                velocity.vx = unit->real.vx - target->real.vx;
                velocity.vx = (velocity.vx - velocity.vx / 8) % 0xe000;
                velocity.vz = unit->real.vz - target->real.vz;
                velocity.vz = (velocity.vz - velocity.vz / 8) % 0xe000;
                velocity.vy = 0;
            } else {
                velocity.vx = (unit->screen.vx - (unit->battle_data->action_target_x * 28 + 0xe)) << 12;
                velocity.vx = (velocity.vx - velocity.vx / 8) % 0xe000;
                velocity.vz = (unit->screen.vz - (unit->battle_data->action_target_y * 28 + 0xe)) << 12;
                velocity.vz = (velocity.vz - velocity.vz / 8) % 0xe000;
                velocity.vy = 0;
            }
            velocity.vx /= timer;
            velocity.vy /= timer;
            velocity.vz /= timer;
            unit->distortion_phase++;
        } else {
            unit->distortion_animation_id = 0;
            break;
        }
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vz += velocity.vz;
        } else {
            unit->distortion_animation_id = 0;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    battle_unit_set_screen_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}

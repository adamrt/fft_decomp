#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

/*
 * Step a projectile from `origin` along `delta` in 0x2000 increments of the
 * `distance` total, testing each position against the obstacle list. Stores
 * the last step's movement and the final position, sets g_battle_effect_trajectory_step_count to a
 * quarter of the steps taken, and returns 1 when the path is clear or 0 when
 * an obstacle stopped it. The `step` variable keeps the target's register
 * divide; a literal 0x2000 folds to a shift.
 */
s32 battle_effect_trace_projectile_path(VECTOR* delta, SVECTOR* origin, s32* distance, void* obstacles) {
    VECTOR position;
    VECTOR previous;
    s32 step;
    s32 steps;
    s32 progress;
    s32 hit;
    s32 i;

    step = 0x2000;
    steps = *distance / step;
    progress = 0;
    position.vx = origin->vx;
    hit = 0;
    position.vy = origin->vy;
    i = 0;
    position.vz = origin->vz;
    for (; i < steps; i++) {
        previous = position;
        progress += step;
        battle_effect_find_projectile_xyz_along_trajectory((s32*)delta, *distance, progress, (s32*)&position);
        position.vx += origin->vx;
        position.vy += origin->vy;
        position.vz += origin->vz;
        hit = battle_effect_test_position_for_obstacle(
            obstacles, &position, &g_battle_effect_trajectory_hit_unit_id, &g_battle_effect_trajectory_tile);
        if (hit != 0) {
            break;
        }
    }
    g_battle_effect_trajectory_step.vx = position.vx - previous.vx;
    g_battle_effect_trajectory_step.vy = position.vy - previous.vy;
    g_battle_effect_trajectory_step.vz = position.vz - previous.vz;
    g_battle_effect_trajectory_position.vx = position.vx;
    g_battle_effect_trajectory_position.vy = position.vy;
    g_battle_effect_trajectory_position.vz = position.vz;
    if (hit != 0) {
        g_battle_effect_trajectory_step_count = i / 4;
        return 0;
    }
    g_battle_effect_trajectory_step_count = steps / 4;
    return 1;
}

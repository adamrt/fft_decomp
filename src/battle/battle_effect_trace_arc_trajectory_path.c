#include "fft/battle.h"
#include "psx/gte.h"
#include "psx/types.h"

/*
 * Step a lobbed projectile along a ballistic arc. The launch pitch comes from
 * ratan2(height, ONE) and is published in both the arc descriptor and
 * g_battle_effect_arctan_angle_mod; the per-step advance is the horizontal
 * component of the gravity impulse. Each step rotates the local (progress,
 * -trajectory term) offset into world space through the yaw matrix and tests
 * the result against the obstacle list, exactly as
 * battle_effect_trace_projectile_path does for straight shots. On a clear
 * path g_battle_effect_trajectory_step_count holds the full step count and the result is 1; on a hit it
 * holds the steps taken and the result is 0.
 *
 * `hit` is deliberately left uninitialised: the target reads $s6 after a
 * zero-step loop without ever writing it.
 */
s32 battle_effect_trace_arc_trajectory_path(
    SVECTOR* origin, s32* height, s32* distance, battle_effect_arc_t* arc, void* obstacles) {
    SVECTOR rotation;
    SVECTOR step;
    VECTOR position;
    VECTOR previous;
    MATRIX matrix;
    s32 angle;
    s32 velocity;
    s32 advance;
    s32 steps;
    s32 progress;
    s32 term;
    s32 hit;
    s32 i;

    angle = ratan2(*height, ONE);
    arc->pitch = angle;
    g_battle_effect_arctan_angle_mod = angle;
    progress = 0;
    /* The definition returns void; this site consumes the returned impulse. */
    velocity = battle_effect_calculate_launch_velocity() >> 8;
    advance = (rcos(arc->pitch) * velocity) >> 4;
    if (advance != 0) {
        steps = ((advance + *distance) - 1) / advance;
    } else {
        main_noop_800449f8(1, 0);
        steps = 0;
    }

    rotation.vz = 0;
    rotation.vx = 0;
    rotation.vy = arc->yaw;
    RotMatrix(&rotation, &matrix);
    position.vx = matrix.t[0] = origin->vx;
    i = 0;
    position.vy = matrix.t[1] = origin->vy;
    position.vz = matrix.t[2] = origin->vz;
    SetRotMatrix(&matrix);
    SetTransMatrix(&matrix);
    step.vz = 0;
    for (; i < steps; i++) {
        previous = position;
        progress += advance;
        term = battle_effect_calculate_arc_trajectory_term(*height, progress);
        step.vx = progress >> 12;
        step.vy = -term >> 12;
        RotTrans(&step, &position, (long*)&position.pad);
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
        g_battle_effect_trajectory_step_count = i;
        return 0;
    }
    g_battle_effect_trajectory_step_count = steps;
    return 1;
}

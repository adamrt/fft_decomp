#include "fft/battle_effect.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/*
 * Step a lobbed projectile along a ballistic arc. The launch pitch comes from
 * ratan2(height, ONE) and is published in both the arc descriptor and
 * g_effect_arctan_angle_mod; the per-step advance is the horizontal
 * component of the gravity impulse. Each step rotates the local (progress,
 * -trajectory term) offset into world space through the yaw matrix and tests
 * the result against the obstacle list, exactly as
 * effect_trace_projectile_path does for straight shots. On a clear
 * path g_effect_trajectory_step_count holds the full step count and the result is 1; on a hit it
 * holds the steps taken and the result is 0.
 *
 * `hit` is deliberately left uninitialized: the target reads $s6 after a
 * zero-step loop without ever writing it.
 */
s32 effect_trace_arc_trajectory_path(
    SVECTOR* origin, s32* height, s32* distance, SVECTOR* arc, effect_obstacle_unit_list_t* obstacles) {
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
    arc->vz = angle;
    g_effect_arctan_angle_mod = angle;
    progress = 0;
    /* The definition returns void; this site consumes the returned impulse. */
    velocity = effect_calculate_launch_velocity() >> 8;
    advance = (rcos(arc->vz) * velocity) >> 4;
    if (advance != 0) {
        steps = ((advance + *distance) - 1) / advance;
    } else {
        main_system_report_stub(1, 0);
        steps = 0;
    }

    rotation.vz = 0;
    rotation.vx = 0;
    rotation.vy = arc->vy;
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
        term = effect_calculate_arc_trajectory_term(*height, progress);
        step.vx = progress >> 12;
        step.vy = -term >> 12;
        RotTrans(&step, &position, &position.pad);
        hit = effect_test_position_for_obstacle(
            obstacles, &position, &g_effect_trajectory_hit_unit_id, &g_effect_trajectory_tile);
        if (hit != 0) {
            break;
        }
    }

    setVector(
        &g_effect_trajectory_step, position.vx - previous.vx, position.vy - previous.vy, position.vz - previous.vz);
    copyVector(&g_effect_trajectory_position, &position);
    if (hit != 0) {
        g_effect_trajectory_step_count = i;
        return 0;
    }
    g_effect_trajectory_step_count = steps;
    return 1;
}

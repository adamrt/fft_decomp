#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"

/*
 * Secondary-effect handler for bow and crossbow arrows.
 *
 * Phase 1 derives the arrow's heading, launch pitch and per-step gravity from
 * the trajectory origin/destination, builds its rotation matrix and allocates
 * the polygon work buffer. Phase 2 advances one step along the arc, orients
 * the arrow along the local slope and draws it with effect_draw_projectile_model; the last
 * step or a positive height ends the flight. Phase 3 returns 0. An unknown
 * phase returns `result` uninitialized, as the target returns $s6 unwritten.
 */
s32 effect_arrow_arc_secondary_handler(void) {
    SVECTOR rotation;
    VECTOR delta;
    VECTOR position;
    SVECTOR angles;
    VECTOR scale;
    SVECTOR spin;
    effect_arrow_buffer_t* work;
    s32 velocity;
    s32 term;
    s32 previous;
    s32 next;
    s32 parameter;
    s32 i;
    s32 j;
    s32 result;
    u8 flags;

    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        delta.vx = g_effect_trajectory_destination.vx - g_effect_trajectory_origin.vx;
        delta.vz = g_effect_trajectory_destination.vz - g_effect_trajectory_origin.vz;
        delta.vy = g_effect_trajectory_origin.vy - g_effect_trajectory_destination.vy;
        g_effect_arrow_arc_length = SquareRoot12((delta.vx * delta.vx + delta.vz * delta.vz) * ONE);
        g_effect_arrow_arc_yaw = ratan2(-delta.vz, delta.vx);
        result = 1;
        g_effect_arrow_arc_caster_relative_yaw = g_effect_arrow_arc_yaw
            - (((battle_unit_get_facing_quadrant_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id)
                    + 1)
                   & 3)
                << 10);
        g_effect_arrow_arc_height = g_effect_arc_trajectory_height;
        g_effect_arrow_arc_launch_angle = ratan2(g_effect_arc_trajectory_height, ONE);
        g_effect_arrow_arc_distance = 0;
        velocity = effect_calculate_launch_velocity() >> 8;
        g_effect_arrow_arc_distance_step = (rcos(g_effect_arrow_arc_launch_angle) * velocity) >> 4;
        g_effect_projectile_step_count = g_effect_trajectory_step_count;
        rotation.vz = 0;
        rotation.vx = 0;
        rotation.vy = g_effect_arrow_arc_yaw;
        RotMatrix(&rotation, &g_effect_projectile_matrix);
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        if (g_effect_current_secondary->allocation != 0) {
            main_heap_free(g_effect_current_secondary->allocation);
        }
        work = main_heap_alloc(0xc88);
        g_effect_current_secondary->allocation = work;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 4; j++) {
                SetPolyG3(&work->g3[i][j]);
            }
            for (j = 0; j < 13; j++) {
                SetPolyG4(&work->g4[i][j]);
            }
        }
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        previous = -effect_calculate_arc_trajectory_term(g_effect_arrow_arc_height, g_effect_arrow_arc_distance);
        g_effect_arrow_arc_distance += g_effect_arrow_arc_distance_step;
        term = -effect_calculate_arc_trajectory_term(g_effect_arrow_arc_height, g_effect_arrow_arc_distance);
        next = -effect_calculate_arc_trajectory_term(
            g_effect_arrow_arc_height, g_effect_arrow_arc_distance + g_effect_arrow_arc_distance_step);
        setVector(
            &angles, 0, g_effect_arrow_arc_yaw, ratan2(next - previous, g_effect_arrow_arc_distance_step * 2) + 0x400);
        SetRotMatrix(&g_effect_projectile_matrix);
        SetTransMatrix(&g_effect_projectile_matrix);
        rotation.vy = term >> 12;
        rotation.vz = 0;
        rotation.vx = g_effect_arrow_arc_distance >> 12;
        RotTrans(&rotation, &delta, &delta.pad);
        setVector(&position, g_effect_trajectory_origin.vx + delta.vx, g_effect_trajectory_origin.vy + delta.vy,
            g_effect_trajectory_origin.vz + delta.vz);
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        setVector(&rotation, g_effect_trajectory_origin.vx + delta.vx, g_effect_trajectory_origin.vy + delta.vy,
            g_effect_trajectory_origin.vz + delta.vz);
        RotTrans(&rotation, &delta, &delta.pad);
        scale.vz = 0x40;
        scale.vy = 0x40;
        scale.vx = 0x40;
        parameter = g_effect_secondary_animations[g_effect_current_secondary->animation_id].parameter;
        spin.vz = 0;
        spin.vy = 0;
        spin.vx = 0;
        effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        if (g_effect_projectile_step_count == 2 && g_effect_trajectory_hit_unit_id != -1
            && g_effect_current_secondary->target_count != 0) {
            battle_unit_call_bow_hardcoding_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id,
                g_effect_current_secondary->target.record.data.unit.runtime_id);
        }
        if (--g_effect_projectile_step_count == 0 || position.vy >= 0) {
            g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
            flags = g_effect_current_secondary->target.record.data.unit.result_animation;
            if (flags == EFFECT_HIT_MISSED || flags - 4 < 2U || g_effect_trajectory_hit_unit_id == -1) {
                D_801B8B98 = 0x80000000;
                effect_end_secondary_on_current_unit();
            }
        }
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

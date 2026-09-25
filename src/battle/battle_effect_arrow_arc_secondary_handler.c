#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "fft/main_heap.h"
#include "psx/gpu.h"
#include "psx/gte.h"

extern s32 battle_effect_calculate_arc_trajectory_term(s32 height, s32 progress);

/*
 * Secondary-effect handler for bow and crossbow arrows.
 *
 * Phase 1 derives the arrow's heading, launch pitch and per-step gravity from
 * the trajectory origin/destination, builds its rotation matrix and allocates
 * the polygon work buffer. Phase 2 advances one step along the arc, orients
 * the arrow along the local slope and draws it through 0x801ae340; the last
 * step or a positive height ends the flight. Phase 3 returns 0. An unknown
 * phase returns `result` uninitialised, as the target returns $s6 unwritten.
 */
s32 battle_effect_arrow_arc_secondary_handler(void) {
    SVECTOR rotation;
    VECTOR delta;
    VECTOR position;
    battle_effect_rotation_vector_t angles;
    VECTOR scale;
    battle_effect_rotation_vector_t spin;
    battle_effect_arrow_buffer_t* work;
    s32 phase;
    s32 velocity;
    s32 term;
    s32 previous;
    s32 next;
    s32 parameter;
    s32 i;
    s32 j;
    s32 result;
    u8 flags;

    phase = g_battle_effect_current_secondary->phase;
    switch ((u32)phase) {
    case BATTLE_SECONDARY_EFFECT_INITIALIZING:
        delta.vx = g_battle_effect_trajectory_destination.vx - g_battle_effect_trajectory_origin.vx;
        delta.vz = g_battle_effect_trajectory_destination.vz - g_battle_effect_trajectory_origin.vz;
        delta.vy = g_battle_effect_trajectory_origin.vy - g_battle_effect_trajectory_destination.vy;
        g_battle_effect_arrow_arc_length = SquareRoot12((delta.vx * delta.vx + delta.vz * delta.vz) << 12);
        g_battle_effect_arrow_arc_yaw = ratan2(-delta.vz, delta.vx);
        result = 1;
        g_battle_effect_arrow_arc_caster_relative_yaw = g_battle_effect_arrow_arc_yaw
            - (((battle_unit_get_facing_quadrant_by_misc_id(
                     (u8)g_battle_effect_current_secondary->caster_block.values[1])
                    + 1)
                   & 3)
                << 10);
        g_battle_effect_arrow_arc_height = g_battle_effect_arc_trajectory_height;
        g_battle_effect_arrow_arc_launch_angle = ratan2(g_battle_effect_arc_trajectory_height, ONE);
        g_battle_effect_arrow_arc_distance = 0;
        velocity = battle_effect_calculate_launch_velocity() >> 8;
        g_battle_effect_arrow_arc_distance_step = (rcos(g_battle_effect_arrow_arc_launch_angle) * velocity) >> 4;
        g_battle_effect_projectile_step_count = g_battle_effect_trajectory_step_count;
        rotation.vz = 0;
        rotation.vx = 0;
        rotation.vy = g_battle_effect_arrow_arc_yaw;
        RotMatrix(&rotation, &g_battle_effect_projectile_matrix);
        g_battle_effect_current_secondary->phase = BATTLE_SECONDARY_EFFECT_EXECUTING;
        if (g_battle_effect_current_secondary->allocation != 0) {
            main_heap_free(g_battle_effect_current_secondary->allocation);
        }
        work = main_heap_alloc(0xc88);
        g_battle_effect_current_secondary->allocation = work;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 4; j++) {
                SetPolyG3(&work->g3[i][j]);
            }
            for (j = 0; j < 13; j++) {
                SetPolyG4(&work->g4[i][j]);
            }
        }
        break;
    case BATTLE_SECONDARY_EFFECT_EXECUTING:
        previous = -battle_effect_calculate_arc_trajectory_term(
            g_battle_effect_arrow_arc_height, g_battle_effect_arrow_arc_distance);
        g_battle_effect_arrow_arc_distance += g_battle_effect_arrow_arc_distance_step;
        term = -battle_effect_calculate_arc_trajectory_term(
            g_battle_effect_arrow_arc_height, g_battle_effect_arrow_arc_distance);
        next = -battle_effect_calculate_arc_trajectory_term(g_battle_effect_arrow_arc_height,
            g_battle_effect_arrow_arc_distance + g_battle_effect_arrow_arc_distance_step);
        angles.x = 0;
        angles.y = g_battle_effect_arrow_arc_yaw;
        angles.z = ratan2(next - previous, g_battle_effect_arrow_arc_distance_step * 2) + 0x400;
        SetRotMatrix(&g_battle_effect_projectile_matrix);
        SetTransMatrix(&g_battle_effect_projectile_matrix);
        rotation.vy = term >> 12;
        rotation.vz = 0;
        rotation.vx = g_battle_effect_arrow_arc_distance >> 12;
        RotTrans(&rotation, &delta, (long*)&delta.pad);
        position.vx = g_battle_effect_trajectory_origin.vx + delta.vx;
        position.vy = g_battle_effect_trajectory_origin.vy + delta.vy;
        position.vz = g_battle_effect_trajectory_origin.vz + delta.vz;
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        rotation.vx = g_battle_effect_trajectory_origin.vx + delta.vx;
        rotation.vy = g_battle_effect_trajectory_origin.vy + delta.vy;
        rotation.vz = g_battle_effect_trajectory_origin.vz + delta.vz;
        RotTrans(&rotation, &delta, (long*)&delta.pad);
        scale.vz = 0x40;
        scale.vy = 0x40;
        scale.vx = 0x40;
        parameter = g_battle_effect_secondary_animations[g_battle_effect_current_secondary->animation_id].parameter;
        spin.z = 0;
        spin.y = 0;
        spin.x = 0;
        battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        if (g_battle_effect_projectile_step_count == 2 && g_battle_effect_trajectory_hit_unit_id != -1
            && g_battle_effect_current_secondary->target_count != 0) {
            battle_unit_call_bow_hardcoding_by_misc_id((u8)g_battle_effect_current_secondary->caster_block.values[1],
                g_battle_effect_current_secondary->target.fields.target_id);
        }
        if (--g_battle_effect_projectile_step_count == 0 || position.vy >= 0) {
            g_battle_effect_current_secondary->phase = BATTLE_SECONDARY_EFFECT_FINALIZING;
            flags = g_battle_effect_current_secondary->target.fields.target_byte_1d;
            if (flags == 3 || flags - 4 < 2U || g_battle_effect_trajectory_hit_unit_id == -1) {
                D_801B8B98 = 0x80000000;
                battle_effect_end_secondary_on_current_unit();
            }
        }
        result = 1;
        break;
    case BATTLE_SECONDARY_EFFECT_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

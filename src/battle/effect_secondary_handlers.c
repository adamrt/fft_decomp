/* Effect secondary updates. Its start is a cut chosen by topic; its end is a boundary the binary proves. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/map.h"
#include "fft/unit.h"
#include "psx/gte_inline.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Secondary effect function 0x00 in g_effect_secondary_handlers: its
 * result of 0 makes effect_update_secondary_effects end the effect at
 * once. */
s32 battle_return_zero_801b0fec(void) {
    return 0;
}

void battle_noop_801b0ff4(void) {
}

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
s32 battle_effect_arrow_arc_secondary_handler(void) {
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
        velocity = battle_effect_calculate_launch_velocity() >> 8;
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
        previous = -battle_effect_calculate_arc_trajectory_term(g_effect_arrow_arc_height, g_effect_arrow_arc_distance);
        g_effect_arrow_arc_distance += g_effect_arrow_arc_distance_step;
        term = -battle_effect_calculate_arc_trajectory_term(g_effect_arrow_arc_height, g_effect_arrow_arc_distance);
        next = -battle_effect_calculate_arc_trajectory_term(
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
        battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        if (g_effect_projectile_step_count == 2 && g_effect_trajectory_hit_unit_id != -1
            && g_effect_current_secondary->target_count != 0) {
            battle_unit_call_bow_hardcoding_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id,
                g_effect_current_secondary->target.record.data.unit.runtime_id);
        }
        if (--g_effect_projectile_step_count == 0 || position.vy >= 0) {
            g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
            flags = g_effect_current_secondary->target.record.data.unit.result_animation;
            if (flags == EFFECT_HIT_MISSED || flags - 4 < 2U || g_effect_trajectory_hit_unit_id == -1) {
                g_battle_unknown_801b8b98 = 0x80000000;
                battle_effect_end_secondary_on_current_unit();
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

/* 0x28-byte heap state kept in the secondary effect's allocation. */
typedef struct effect_burst_state {
    s32 frame;    /* 0x00 */
    s32 done;     /* 0x04 */
    s32 count_a;  /* 0x08 */
    u8 ids_a[8];  /* 0x0c; group 0 slots */
    s32 count_b;  /* 0x14 */
    u8 ids_b[16]; /* 0x18; group 1 or 9 slots */
} effect_burst_state_t;

/* Secondary effect function 0x02 in g_effect_secondary_handlers: the burst of a weapon
 * hit (EFFECT_SECONDARY_ANIMATION_WEAPON_HIT). */
s32 battle_effect_weapon_hit_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_burst_state_t* state;
    effect_slot_t* slot;
    s32 unused[8];
    vec3s32_t gravity;
    VECTOR zoom;
    VECTOR offset;
    s32 result;
    s32 group;
    s32 j;
    s32 done;
    s32 i;
    u8 id;

    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        if (effect->target.record.data.unit.result_animation == EFFECT_HIT_NO_EFFECT) {
            return 0;
        }
        if (effect->allocation != 0) {
            main_heap_free(effect->allocation);
        }
        state = main_heap_alloc(sizeof(effect_burst_state_t));
        g_effect_current_secondary->allocation = state;
        state->frame = 0;
        state->done = 0;
        state->count_a = 0;
        state->count_b = 0;
        for (i = 0; i < 8; i++) {
            state->ids_a[i] = 0;
        }
        for (i = 0; i < 16; i++) {
            state->ids_b[i] = 0;
        }
        result = 1;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        battle_effect_copy_camera_angles_and_world_coords();
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        state = effect->allocation;
        group = EFFECT_PARTICLE_GROUP_WEAPON_HIT_B;
        if (effect->target.record.data.unit.result_animation == EFFECT_HIT_MISSED) {
            group = EFFECT_PARTICLE_GROUP_WEAPON_HIT_B_MISSED;
        }
        if (group == EFFECT_PARTICLE_GROUP_WEAPON_HIT_B) {
            if (state->frame == 0) {
                battle_gfx_apply_unit_palette_modulation_to_units(
                    MAP_COLOR_MODE_BASE_HALF, 0, effect->target.record.data.unit.runtime_id, 0x1f, 0x1f, 0x1f);
            }
            if (state->frame == group) {
                battle_gfx_apply_unit_palette_modulation_to_units(
                    MAP_COLOR_MODE_RESTORE, 2, g_effect_current_secondary->target.record.data.unit.runtime_id, 0, 0, 0);
            }
        }
        if (state->frame < 4) {
            if (state->count_a < 8) {
                for (i = 0; i < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_WEAPON_HIT_A].spawns_per_frame;
                    i++) {
                    for (j = 0; j < 8; j++) {
                        if (state->ids_a[j] == 0) {
                            break;
                        }
                    }
                    state->ids_a[j] = battle_effect_alloc_slot();
                    if (state->ids_a[j] == 0) {
                        break;
                    }
                    slot = &g_effect_slots[state->ids_a[j]];
                    slot->life = battle_effect_spawn_particle_motion(
                        &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_WEAPON_HIT_A], &slot->motion);
                    battle_effect_init_trap_animation(EFFECT_PARTICLE_GROUP_WEAPON_HIT_A,
                        g_effect_current_secondary->parameter + MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 0), slot);
                    state->count_a++;
                    if (state->count_a == 8) {
                        break;
                    }
                }
            }
            if (state->frame < 4 && state->count_b < 16) {
                for (i = 0; i < g_effect_trap_particle_configs[group].spawns_per_frame; i++) {
                    for (j = 0; j < 16; j++) {
                        if (state->ids_b[j] == 0) {
                            break;
                        }
                    }
                    state->ids_b[j] = battle_effect_alloc_slot();
                    if (state->ids_b[j] == 0) {
                        break;
                    }
                    slot = &g_effect_slots[state->ids_b[j]];
                    slot->life
                        = battle_effect_spawn_particle_motion(&g_effect_trap_particle_configs[group], &slot->motion);
                    battle_effect_init_trap_animation(
                        EFFECT_PARTICLE_GROUP_WEAPON_HIT_B, MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 10), slot);
                    slot->color.r = slot->color.g = slot->color.b = 0xff;
                    state->count_b++;
                    if (state->count_b == 16) {
                        break;
                    }
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        for (i = 0; i < 8; i++) {
            id = state->ids_a[i];
            if (id != 0 && battle_effect_update_slot(id) == 0) {
                battle_effect_free_slot(id);
                state->ids_a[i] = 0;
                state->count_a--;
            }
        }
        for (i = 0; i < 16; i++) {
            id = state->ids_b[i];
            if (id != 0 && battle_effect_update_slot(id) == 0) {
                battle_effect_free_slot(id);
                state->ids_b[i] = 0;
                state->count_b--;
            }
        }
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        if (g_effect_current_secondary->target.record.data.unit.result_animation == EFFECT_HIT_CRITICAL) {
            if (state->frame == 0) {
                zoom.vx = zoom.vy = zoom.vz = -0x80;
                battle_camera_execute_zoom_command(EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 2, &zoom);
            }
            if (state->frame == 2) {
                zoom.vx = zoom.vy = zoom.vz = 0x2e4;
                battle_camera_execute_zoom_command(EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 2, &zoom);
            }
            if (state->frame == 4) {
                zoom.vx = zoom.vy = zoom.vz = 0;
                battle_camera_execute_zoom_command(
                    EFFECT_CAMERA_SOURCE_SAVED | EFFECT_CAMERA_MODE_COSINE, 0, 12, &zoom);
                offset.vx = offset.vy = offset.vz = 4;
                battle_camera_execute_position_command(
                    EFFECT_CAMERA_SOURCE_GIVEN | EFFECT_CAMERA_MODE_SHAKE_DECAYING, 0, 9, &offset);
            }
        }
        done = state->done;
        result = done != 1;
        state->frame++;
        if (state->count_a == 0 && state->count_b == 0 && state->frame >= 31) {
            state->done = done + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x03 in g_effect_secondary_handlers: the
 * particles of EFFECT_SECONDARY_ANIMATION_GOLEM.
 *
 * Initializing clears the counters. Executing starts this frame's share of the
 * Golem particle group with CLUT 13 of the weapons row and, like Finalizing,
 * steps the live slots under the secondary-effect inertia threshold and a gravity
 * of (0, ONE, 0), then puts back the effect file's own values. Once no slot is
 * live and the timer has passed the group's spawn_start_frame, finish_timer
 * counts up; the handler returns 0 on the frame after that begins, and 1 until
 * then. */
s32 battle_effect_golem_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    s32 unused[6];
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_GOLEM, 13);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_GOLEM);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        current = g_effect_current_secondary;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_GOLEM].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* The target compares a line's two endpoints as 32-bit values. */
#define LINE_G2_XY_WORD(line, n) (*(s32*)&line->x##n)

/* Secondary handler for a charge effect: phase 1 loads the animation's
 * parameters and primes the line primitives, phase 2 spawns trails that
 * converge on the caster, and phases 2-3 advance and draw them.
 *
 * The OT link masks `line` before `setaddr(line, getaddr(ot + 1))`: that keeps
 * the 0xffffff mask's loop lifetime short enough for loop.c to leave it in the
 * inner loop, and getaddr's extra mask reference gives it priority over the
 * 0xff000000 mask in local allocation (a1 versus a2 in the target). */
s32 battle_effect_spell_charge_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_charge_state_t* state;
    SVECTOR in;
    s32 unused[4];
    VECTOR out;
    LINE_G2* line;
    SVECTOR* screen;
    effect_slot_t* slot;
    u32* ot;
    s16* offsets;
    s16* offsets_y;
    s32 result;
    s32 index;
    s32 i;
    s32 j;
    s32 k;
    s32 age;
    s32 head;
    s32 fade;
    s32 x;
    s32 y;
    s32 dir;
    s32 over;
    u32 red;
    u32 green;
    u32 blue;
    u32 link;
    effect_slot_t* work;

    effect = g_effect_current_secondary;
    state = &g_effect_secondary_state.charge;
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        index = g_effect_secondary_animations[effect->animation_id].parameter;
        g_effect_current_secondary->parameter = battle_effect_get_element_index(effect->parameter);
        g_effect_secondary_state.charge.speed = g_effect_charge_line_params[index].speed;
        g_effect_secondary_state.charge.spread_range = g_effect_charge_line_params[index].spread_range;
        g_effect_secondary_state.charge.spread_offset = g_effect_charge_line_params[index].spread_offset;
        g_effect_secondary_state.charge.duration = g_effect_charge_line_params[index].duration;
        g_effect_secondary_state.charge.radius = g_effect_charge_line_params[index].radius;
        g_effect_secondary_state.charge.spawn_interval = g_effect_charge_line_params[index].spawn_interval;
        g_effect_secondary_state.charge.color = g_effect_current_secondary->parameter;
        g_effect_secondary_state.charge.max_trails = g_effect_charge_line_params[index].max_trails;
        g_effect_secondary_state.charge.fade_row = g_effect_charge_line_params[index].fade_row;
        SetDrawMode(&state->mode[0], 1, 1, 0x20, 0);
        SetDrawMode(&state->mode[1], 1, 1, 0x20, 0);
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 96; j++) {
                SetLineG2(&state->lines[i][j]);
                SetSemiTrans(&state->lines[i][j], 1);
            }
        }
        for (i = 0; i < 16; i++) {
            state->trails[i].slot_id = 0;
        }
        state->count.trails = 0;
        state->count.auxiliary_particles = 0;
        state->count.particles = 0;
        for (i = 0; i < 2; i++) {
            state->ids_a[i] = 0;
        }
        for (i = 0; i < 14; i++) {
            state->ids_b[i] = 0;
        }
        screen = battle_unit_get_world_coords_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id);
        i = battle_gfx_get_unit_spritesheet_height_by_runtime_id(
                g_effect_current_secondary->caster.data.unit.runtime_id)
            + 8;
        g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE].position.y
            = g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE_13].position.y = -i;
        in.vx = screen->vx;
        in.vz = screen->vz;
        in.vy = screen->vy - i;
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        RotTrans(&in, &out, &out.pad);
        state->angle = 0;
        state->done = 0;
        state->x = out.vx;
        state->y = out.vy;
        suzuki_sound_play_sfx(MAIN_SFX_SPELL_CHARGE);
        result = 1;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        if (g_effect_secondary_state.charge.done == 0
            && g_effect_secondary_state.charge.count.trails < g_effect_secondary_state.charge.max_trails
            && rand() % g_effect_secondary_state.charge.spawn_interval == 0) {
            for (i = 0; i < 16; i++) {
                if (state->trails[i].slot_id == 0) {
                    j = state->trails[i].slot_id = battle_effect_alloc_slot();
                    if (j == 0) {
                        break;
                    }
                    state->count.trails++;
                    k = (rand() & 0x1ff) + state->angle;
                    state->angle += 0x571;
                    g_effect_slots[j].motion.position.x = rcos(k) * state->radius + (state->x * ONE);
                    g_effect_slots[j].motion.position.y = rsin(k) * state->radius + (state->y * ONE);
                    k = k + rand() % state->spread_range - state->spread_offset - 0x800;
                    g_effect_slots[j].motion.velocity.x = rcos(k) * state->speed;
                    g_effect_slots[j].motion.velocity.y = rsin(k) * state->speed;
                    g_effect_slots[j].life = 1;
                    for (k = 0; k < 7; k++) {
                        state->trails[i].points[k].vx = g_effect_slots[j].motion.position.x >> 12;
                        state->trails[i].points[k].vy = g_effect_slots[j].motion.position.y >> 12;
                    }
                    state->trails[i].age = 0;
                    break;
                }
            }
        }
        if (state->count.particles < 14) {
            for (i = 0; i < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE].spawns_per_frame; i++) {
                for (j = 0; j < 14; j++) {
                    if (state->ids_b[j] == 0) {
                        break;
                    }
                }
                state->ids_b[j] = battle_effect_alloc_slot();
                if (state->ids_b[j] == 0) {
                    break;
                }
                slot = &g_effect_slots[state->ids_b[j]];
                slot->life = battle_effect_spawn_particle_motion(
                    &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE], &slot->motion);
                battle_effect_init_trap_animation(
                    EFFECT_PARTICLE_GROUP_SPELL_CHARGE, MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 15), slot);
                state->count.particles++;
                if (state->count.particles == 14) {
                    break;
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        for (i = 0; i < 2; i++) {
            j = state->ids_a[i];
            if (j != 0 && battle_effect_update_slot(j) == 0) {
                battle_effect_free_slot(j);
                state->ids_a[i] = 0;
                state->count.auxiliary_particles--;
            }
        }
        for (i = 0; i < 14; i++) {
            j = state->ids_b[i];
            if (j != 0 && battle_effect_update_slot(j) == 0) {
                battle_effect_free_slot(j);
                state->ids_b[i] = 0;
                state->count.particles--;
            }
        }
        red = g_effect_trap_colors[state->color].r;
        green = g_effect_trap_colors[state->color].g;
        blue = g_effect_trap_colors[state->color].b;
        offsets = &g_effect_charge_line_direction_offsets[0].vx;
        offsets_y = &g_effect_charge_line_direction_offsets[0].vy;
        for (i = 0; i < 16; i++) {
            j = state->trails[i].slot_id;
            if (j != 0) {
                work = &g_effect_slots[j];
                work->motion.position.x = x = g_effect_slots[j].motion.position.x + g_effect_slots[j].motion.velocity.x;
                work->motion.position.y = y = g_effect_slots[j].motion.position.y + g_effect_slots[j].motion.velocity.y;
                age = ++state->trails[i].age;
                head = age % 7;
                if (age <= state->duration) {
                    state->trails[i].points[head].vx
                        = battle_effect_interpolate_s32_cos(x >> 12, state->x, state->duration, age);
                    fade = 6;
                    state->trails[i].points[head].vy
                        = battle_effect_interpolate_s32_cos(y >> 12, state->y, state->duration, age);
                } else {
                    state->trails[i].points[head].vx = state->x;
                    state->trails[i].points[head].vy = state->y;
                    over = age - 6;
                    fade = state->duration - over;
                    if (fade < 0) {
                        fade = 0;
                    }
                }
                for (k = 0; k < 6; k++) {
                    line = &state->lines[g_effect_buffer_index][i * 6 + k];
                    line->x0 = state->trails[i].points[head].vx;
                    line->y0 = state->trails[i].points[head].vy;
                    setRGB0(line, red * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8,
                        green * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8,
                        blue * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8);
                    if (--head < 0) {
                        head = 6;
                    }
                    age--;
                    if (fade != 0) {
                        fade--;
                    }
                    if (age < 0) {
                        fade = 0;
                    }
                    line->x1 = state->trails[i].points[head].vx;
                    line->y1 = state->trails[i].points[head].vy;
                    setRGB1(line, red * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8,
                        green * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8,
                        blue * g_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8);
                    if (LINE_G2_XY_WORD(line, 0) != LINE_G2_XY_WORD(line, 1)) {
                        dir = (ratan2(line->y1 - line->y0, line->x1 - line->x0) + 0x900) / 512;
                        line->x1 += offsets[dir * 2];
                        line->y1 += offsets_y[dir * 2];
                        ot = g_effect_polygon_depth_data;
                        link = (u32)line & 0xffffff;
                        setaddr(line, getaddr(ot + 1));
                        ot[1] = (ot[1] & 0xff000000) | link;
                    }
                }
                if (state->trails[i].age >= state->duration + 7) {
                    battle_effect_free_slot(j);
                    state->trails[i].slot_id = 0;
                    state->count.trails--;
                }
            }
        }
        AddPrim(g_effect_polygon_depth_data + 1, &state->mode[g_effect_buffer_index]);
        if (g_effect_current_secondary->phase == EFFECT_SECONDARY_PHASE_EXECUTING
            || (state->count.trails != 0 || state->count.auxiliary_particles != 0 || state->count.particles != 0)) {
            result = 1;
        } else {
            suzuki_sound_stop_sfx_channels(MAIN_SFX_SPELL_CHARGE);
            result = 0;
        }
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x05: an erroneous
 * secondary animation that only steps its state so that a null animation can
 * finish cleanly.  Returns 1 to keep animating, 0 to stop.
 */
s32 battle_effect_null_secondary_handler(void) {
    s32 result;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x06 (Red Orbs) in g_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize and play sound 8, then step the effect slots with a
 * gravity of (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep
 * animating.
 */
s32 battle_effect_red_orbs_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        suzuki_sound_play_sfx(MAIN_SFX_RED_ORBS);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_RED_ORBS, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_RED_ORBS);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        effect = g_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_RED_ORBS].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x07 in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_noop_801b2968(void) {
}

/*
 * Secondary effect function 0x08 (the dust of a fall) in g_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize, then step the effect slots with a gravity of
 * (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep animating.
 */
s32 battle_effect_fall_dust_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_effect_current_secondary->active_count = 0;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->finish_timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_FALL_DUST, 9);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_FALL_DUST);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        effect = g_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_FALL_DUST].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x09 (Splash) in g_effect_secondary_handlers.
 * It shares the fall dust
 * handler's shape; each executing frame first lifts group 7 to the water
 * surface of the target's tile, 12 units per height and depth step above the
 * target's screen z. Returns 1 to keep animating.
 */
s32 battle_effect_splash_secondary_handler(void) {
    effect_secondary_data_t* effect;
    map_tile_t* tile;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_effect_current_secondary->active_count = 0;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->finish_timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        tile = battle_map_get_unit_tile_from_world_coords(
            g_effect_current_secondary->target.record.data.unit.runtime_id);
        g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPLASH].position.y
            = -((tile->height + tile->depth_half_height.depth) * 12)
            - battle_unit_get_world_coords_by_runtime_id(g_effect_current_secondary->target.record.data.unit.runtime_id)
                  ->vy;
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_SPLASH, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_SPLASH);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        effect = g_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPLASH].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x0a in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_noop_801b2bfc(void) {
}

/* Secondary effect function 0x0b in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_noop_801b2c04(void) {
}

/*
 * Secondary effect function 0x0c (Venom Trap) in g_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize and play sound 0x22, then step the effect slots with a
 * gravity of (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep
 * animating.
 */
s32 battle_effect_venom_trap_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        suzuki_sound_play_sfx(MAIN_SFX_VENOM_TRAP);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_VENOM_TRAP, 12);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_VENOM_TRAP);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        effect = g_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_VENOM_TRAP].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x0d (Level Up) in g_effect_secondary_handlers.
 * It shares the venom trap
 * handler's shape but seeds group 5's position_y with -8 and lowers it by 3 per
 * executing frame instead of playing a sound. Returns 1 to keep animating.
 */
s32 battle_effect_level_up_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].position.y = -8;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_LEVEL_UP, 11);
        {
            /* Addressing through a pointer keeps the target lui/addiu pair. */
            s16* rise = &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].position.y;
            *rise -= 3;
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_LEVEL_UP);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        current = g_effect_current_secondary;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0
            && current->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_LEVEL_UP].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x0e in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_noop_801b2e60(void) {
}

enum {
    EFFECT_TRAP_SLOT_COUNT = 2,
    EFFECT_TRAP_CLUT = MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 15),
};

/*
 * Death Poof secondary effect (0x0f) handler for the two-slot trap group 4: it
 * starts this frame's share of the group's effect slots, steps them with a
 * gravity of (0, ONE, 0) and finishes once the spawn state's finish_timer
 * reaches 1.
 *
 * The handler table calls it with no arguments: for a phase outside 1-3 the
 * result is `passthrough`, whatever the caller left in the second argument
 * register.
 */
s32 battle_effect_update_death_poof_secondary(s32 unused_arg0, s32 passthrough) {
    effect_secondary_data_t* effect;
    effect_death_poof_state_t* state;
    s32 unused[6]; /* Reserves the target's 0x58-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s32 spawned;
    s32 i;

    effect = g_effect_current_secondary;
    result = passthrough;
    state = &g_effect_death_poof_states[effect->own_slot_id];
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING: {
        state->count = 0;
        for (i = 0; i < EFFECT_TRAP_SLOT_COUNT; i++) {
            state->ids[i] = 0;
        }
        state->finish_timer = 0;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    }
    case EFFECT_SECONDARY_PHASE_EXECUTING: {
        effect_slot_t* slot;
        s32 i;
        u8 id;

        if (effect->timer >= g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_start_frame
            && effect->timer < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_end_frame) {
            for (spawned = 0;
                spawned < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawns_per_frame;
                spawned++) {
                for (i = 0; i < EFFECT_TRAP_SLOT_COUNT; i++) {
                    if (state->ids[i] == 0) {
                        break;
                    }
                }
                id = battle_effect_alloc_slot();
                state->ids[i] = id;
                if (state->ids[i] == 0) {
                    break;
                }
                slot = &g_effect_slots[state->ids[i]];
                slot->life = battle_effect_spawn_particle_motion(
                    &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF], &slot->motion);
                battle_effect_init_trap_animation(EFFECT_PARTICLE_GROUP_DEATH_POOF, EFFECT_TRAP_CLUT, slot);
                state->count++;
                if (state->count == EFFECT_TRAP_SLOT_COUNT) {
                    break;
                }
            }
        }
    }
    /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING: {
        effect_secondary_data_t* current;
        u8 id;

        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        for (i = 0; i < EFFECT_TRAP_SLOT_COUNT; i++) {
            id = state->ids[i];
            if (id != 0 && battle_effect_update_slot(id) == 0) {
                battle_effect_free_slot(id);
                state->ids[i] = 0;
                state->count--;
            }
        }
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        current = g_effect_current_secondary;
        current->timer = current->timer + 1;
        result = state->finish_timer != 1;
        if (state->count == 0
            && current->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_DEATH_POOF].spawn_start_frame) {
            state->finish_timer = state->finish_timer + 1;
        }
        break;
    }
    }
    return result;
}

/* Secondary effect function 0x10 in g_effect_secondary_handlers: an empty
 * handler. It sets no result, although effect_update_secondary_effects
 * tests one to decide whether the effect ends. */
s32 battle_noop_801b3118(void) {
}

enum {
    EFFECT_TRAP3_SLOT_COUNT = 24,
};

/* Secondary effect function 0x11 (Zodiac Poof): the 24-slot particle group 3.
 *
 * Its spawn state is fixed at 0x801bade0 and its CLUT comes from the table
 * indexed by the effect's parameter, the caster's sprite palette. It follows
 * the same phase structure as the death-poof secondary handler.
 */
s32 battle_effect_zodiac_poof_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_trap_state_24_t* state;
    effect_slot_t* slot;
    s32 unused[6]; /* Reserves the target's 0x58-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s32 spawned;
    s32 i;
    s32 done;
    s16 timer;
    u8 id;
    u16 clut_index;

    effect = g_effect_current_secondary;
    state = &g_effect_secondary_state.zodiac_poof;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        state->count = 0;
        for (spawned = 0; spawned < EFFECT_TRAP3_SLOT_COUNT; spawned++) {
            state->ids[spawned] = 0;
        }
        state->done = 0;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        clut_index = g_effect_zodiac_poof_animations[effect->parameter];
        if (effect->timer >= g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_start_frame
            && effect->timer < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_end_frame) {
            for (spawned = 0;
                spawned < g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawns_per_frame;
                spawned++) {
                for (i = 0; i < EFFECT_TRAP3_SLOT_COUNT; i++) {
                    if (state->ids[i] == 0) {
                        break;
                    }
                }
                id = battle_effect_alloc_slot();
                state->ids[i] = id;
                if (state->ids[i] == 0) {
                    break;
                }
                slot = &g_effect_slots[state->ids[i]];
                slot->life = battle_effect_spawn_particle_motion(
                    &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF], &slot->motion);
                battle_effect_init_trap_animation(
                    EFFECT_PARTICLE_GROUP_ZODIAC_POOF, clut_index + MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 0), slot);
                state->count++;
                if (state->count == EFFECT_TRAP3_SLOT_COUNT) {
                    break;
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        for (spawned = 0; spawned < EFFECT_TRAP3_SLOT_COUNT; spawned++) {
            id = state->ids[spawned];
            if (id != 0 && battle_effect_update_slot(id) == 0) {
                battle_effect_free_slot(id);
                state->ids[spawned] = 0;
                state->count--;
            }
        }
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        {
            effect_secondary_data_t* current_effect;

            current_effect = g_effect_current_secondary;
            timer = current_effect->timer + 1;
            current_effect->timer = timer;
        }
        done = state->done;
        result = done != 1;
        if (state->count == 0
            && timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_ZODIAC_POOF].spawn_start_frame) {
            state->done = done + 1;
        }
        break;
    }
    return result;
}

/* Secondary-effect handler 0x12 (Reflect). The effect's parameter is the
 * frames the shield stays, at least 26: the child timeline passes the
 * effect's spawn_delay.
 *
 * Phase 1 allocates and initializes the work buffer; phase 2 advances a
 * two-entry CLUT wipe near the end of the effect, uploads the CLUT, aims the
 * shield from the target's center toward the caster and hands the transform
 * to effect_draw_projectile_model; phase 3 finishes. The wipe indices are assigned inside the
 * subscript, each to its own variable: a separate statement or a folded
 * `timer - 1 - span` changes the order and association of the index
 * arithmetic. spin_timer keeps the timer load at full width (lh). */
s32 battle_effect_reflect_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[2];
    VECTOR delta;
    VECTOR position;
    SVECTOR angles;
    VECTOR scale;
    SVECTOR spin;
    RECT rect;
    effect_reflect_buffer_t* work;
    SVECTOR* caster;
    SVECTOR* target;
    rgb24_t* colors;
    u8* blue;
    u8* green;
    u8* red;
    s32 span;
    s32 timer;
    s32 lit;
    s32 cleared;
    s32 spin_timer;
    s32 parameter;
    s32 i;
    s32 j;
    s32 result;

    span = g_effect_current_secondary->parameter;
    if (span < 0x1a) {
        span = 0x1a;
    }
    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        if (g_effect_current_secondary->allocation != 0) {
            main_heap_free(g_effect_current_secondary->allocation);
        }
        work = main_heap_alloc(0x798);
        g_effect_current_secondary->allocation = work;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 0x18; j++) {
                SetPolyGT3(&work->prims[i][j]);
                SetSemiTrans(&work->prims[i][j], 1);
            }
        }
        /* Separate channel pointers retain the target's three stride bases. */
        for (i = 0; i < 2; i++) {
            colors = g_effect_reflect_palette_colors;
            red = &colors->r;
            green = &colors->g;
            blue = &colors->b;
            for (j = 0; j < 6; j++) {
                work->clut[i][j] = red[j * sizeof(*colors)] | (green[j * sizeof(*colors)] << 5)
                    | (blue[j * sizeof(*colors)] << 10) | 0x8000;
            }
        }
        result = 1;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        work = g_effect_current_secondary->allocation;
        timer = g_effect_current_secondary->timer;
        if (span < timer && timer < span + 4) {
            work->clut[g_effect_buffer_index][((lit = timer - 1) - span) * 2]
                = work->clut[g_effect_buffer_index][(lit - span) * 2 + 1] = 0xffff;
        }
        if (span + 1 < g_effect_current_secondary->timer) {
            work->clut[g_effect_buffer_index][((cleared = g_effect_current_secondary->timer - 2) - span) * 2]
                = work->clut[g_effect_buffer_index][(cleared - span) * 2 + 1] = 0;
        }
        rect.x = (g_effect_current_secondary->own_slot_id - 1) * 16 + 1;
        rect.y = 0x1fc;
        rect.w = 6;
        rect.h = 1;
        LoadImage(&rect, (u32*)work->clut[g_effect_buffer_index]);
        caster = battle_unit_get_world_coords_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id);
        target = battle_unit_get_world_coords_by_runtime_id(
            g_effect_current_secondary->target.record.data.unit.runtime_id);
        setVector(&delta, caster->vx - target->vx, caster->vy - target->vy, caster->vz - target->vz);
        position.vx = target->vx;
        position.vz = target->vz;
        position.vy = target->vy
            - (s32)battle_gfx_get_unit_spritesheet_height_by_runtime_id(
                  g_effect_current_secondary->target.record.data.unit.runtime_id)
                / 2;
        angles.vy = ratan2(-delta.vz, delta.vx);
        angles.vz = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
        angles.vx = 0;
        scale.vz = 0x140;
        scale.vy = 0x140;
        scale.vx = 0x140;
        parameter = g_effect_secondary_animations[g_effect_current_secondary->animation_id].parameter;
        spin.vz = 0;
        spin.vx = 0;
        spin_timer = g_effect_current_secondary->timer;
        spin.vz = spin_timer << 7;
        battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        effect = g_effect_current_secondary;
        effect->timer++;
        if (span + 4 < effect->timer) {
            effect->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
        }
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x13 (Teleport) in g_effect_secondary_handlers.
 * It shares the golem handler's
 * shape: initialize, then step the effect slots with a gravity of
 * (0, ONE, 0) until finish_timer reaches 1. Returns 1 to keep animating.
 */
s32 battle_effect_teleport_secondary_handler(void) {
    effect_secondary_data_t* effect;
    s32 unused[6]; /* Reserves the target's 0x48-byte frame. */
    vec3s32_t gravity;
    s32 result;
    s16 finish_timer;

    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        g_effect_current_secondary->active_count = 0;
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->finish_timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        battle_effect_start_group(EFFECT_PARTICLE_GROUP_TELEPORT, 11);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(EFFECT_PARTICLE_GROUP_TELEPORT);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        effect = g_effect_current_secondary;
        finish_timer = effect->finish_timer;
        result = finish_timer != 1;
        effect->timer = effect->timer + 1;
        if (effect->active_count == 0
            && effect->timer > g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_TELEPORT].spawn_start_frame) {
            effect->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

/* Secondary effect function 0x14 in g_effect_secondary_handlers: a
 * projectile flying in a straight line. It runs the crossbow bolt (the arrow
 * model), the thrown stone and the thrown item animations.
 *
 * Initializing measures the trajectory, takes the flight time from
 * g_effect_trajectory_step_count and allocates what the projectile draws
 * with: the polygon buffer of the arrow or stone model, or for a thrown item the
 * shuriken model's buffer (items ITEM_ID_SHURIKEN to ITEM_ID_YAGYU_DARKNESS) or
 * else an effect slot holding the item's icon as a quad. Executing moves 0x8000
 * (8 units in Q12) along the line each frame, aims the model along it and draws
 * it: the arrow at scale 0x40, the stone and the shuriken at 0xc0 and spinning by
 * the frames left, any other item as its icon sprite.
 *
 * With two frames left and a unit hit by the trajectory
 * (g_effect_trajectory_hit_unit_id), the target's hit reaction
 * (battle_unit_call_bow_hardcoding_by_runtime_id) starts, unless the item is one of
 * the balls or a consumable (ITEM_ID_POTION to ITEM_ID_PHOENIX_DOWN). When the
 * count runs out the icon slot is freed; a unit target whose result is neither
 * MISSED nor NO_EFFECT releases the requested ability effect at animation speed
 * 2, and otherwise a requested effect is dropped after the hit reaction. The
 * handler then enters Finalizing, whose return of 0 removes it.
 *
 * g_effect_projectile_spin_passes is the count of frames left. result is
 * unset for a phase outside 1-3, and scale and spin for a model other than the
 * arrow or the stone. */
s32 battle_effect_linear_projectile_secondary_handler(void) {
    s32 unused[2];
    VECTOR delta;
    VECTOR position;
    SVECTOR angles;
    VECTOR scale;
    SVECTOR spin;
    SPRT sprite;
    effect_arrow_buffer_t* arrow;
    effect_stone_buffer_t* work12;
    effect_shuriken_buffer_t* work16;
    effect_slot_t* slot;
    s32 parameter;
    s32 item;
    s32 i;
    s32 j;
    s32 result;
    s32 kind;
    POLY_FT4* dst;

    switch (g_effect_current_secondary->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        setVector(&delta, g_effect_trajectory_destination.vx - g_effect_trajectory_origin.vx,
            g_effect_trajectory_destination.vy - g_effect_trajectory_origin.vy,
            g_effect_trajectory_destination.vz - g_effect_trajectory_origin.vz);
        g_effect_projectile_length
            = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
        g_effect_projectile_delta = delta;
        g_effect_projectile_progress = 0;
        g_effect_projectile_speed = 0x8000;
        g_effect_projectile_spin_passes = g_effect_trajectory_step_count;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        if (g_effect_current_secondary->allocation != 0) {
            main_heap_free(g_effect_current_secondary->allocation);
        }
        parameter = g_effect_secondary_animations[g_effect_current_secondary->animation_id].parameter;
        if ((parameter & 0xff) == EFFECT_MODEL_ARROW) {
            arrow = main_heap_alloc(0xc88);
            g_effect_current_secondary->allocation = arrow;
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 4; j++) {
                    SetPolyG3(&arrow->g3[i][j]);
                }
                for (j = 0; j < 13; j++) {
                    SetPolyG4(&arrow->g4[i][j]);
                }
            }
        }
        if ((parameter & 0xff) == EFFECT_MODEL_STONE) {
            work12 = main_heap_alloc(0x2a0);
            g_effect_current_secondary->allocation = work12;
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 12; j++) {
                    SetPolyG3(&work12->g3[i][j]);
                }
            }
        }
        if (parameter == EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC) {
            item = g_effect_current_secondary->used_weapon_id;
            if (item >= ITEM_ID_SHURIKEN && item <= ITEM_ID_YAGYU_DARKNESS) {
                work16 = main_heap_alloc(0x380);
                g_effect_current_secondary->allocation = work16;
                for (i = 0; i < 2; i++) {
                    for (j = 0; j < 16; j++) {
                        SetPolyG3(&work16->g3[i][j]);
                    }
                }
            } else {
                gfx_set_item_icon_sprite(&sprite, item);
                g_effect_current_secondary->slot_ids[0] = battle_effect_alloc_slot();
                slot = &g_effect_slots[g_effect_current_secondary->slot_ids[0]];
                dst = &slot->quads[1];
                SetPolyFT4(&slot->quads[0]);
                slot->quads[0].tpage = 0x1e;
                slot->quads[0].b0 = 0x80;
                slot->quads[0].g0 = 0x80;
                slot->quads[0].r0 = 0x80;
                slot->quads[0].u0 = slot->quads[0].u2 = sprite.u0;
                slot->quads[0].u1 = slot->quads[0].u3 = slot->quads[0].u0 + sprite.w;
                slot->quads[0].v0 = slot->quads[0].v1 = sprite.v0;
                slot->quads[0].v2 = slot->quads[0].v3 = slot->quads[0].v0 + sprite.h;
                slot->quads[0].clut = sprite.clut;
                *dst = slot->quads[0];
            }
        }
        battle_state_set_animation_speed(1);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        g_effect_projectile_progress += g_effect_projectile_speed;
        battle_effect_find_projectile_xyz_along_trajectory(
            &g_effect_projectile_delta, g_effect_projectile_length, g_effect_projectile_progress, &position);
        setVector(&position, g_effect_trajectory_origin.vx + position.vx, g_effect_trajectory_origin.vy + position.vy,
            g_effect_trajectory_origin.vz + position.vz);
        delta.vx = g_effect_trajectory_destination.vx - g_effect_trajectory_origin.vx;
        delta.vz = g_effect_trajectory_destination.vz - g_effect_trajectory_origin.vz;
        delta.vy = g_effect_trajectory_destination.vy - g_effect_trajectory_origin.vy;
        angles.vy = ratan2(-delta.vz, delta.vx);
        angles.vz = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
        angles.vx = 0;
        parameter = g_effect_secondary_animations[g_effect_current_secondary->animation_id].parameter;
        i = parameter & 0xff;
        if (parameter != EFFECT_SECONDARY_PARAMETER_ITEM_GRAPHIC) {
            if (i == EFFECT_MODEL_ARROW) {
                scale.vz = 0x40;
                scale.vy = 0x40;
                scale.vx = 0x40;
                spin.vz = 0;
                spin.vy = 0;
                spin.vx = 0;
            } else if (i == EFFECT_MODEL_STONE) {
                scale.vz = 0xc0;
                scale.vy = 0xc0;
                scale.vx = 0xc0;
                setVector(&spin, 0, g_effect_projectile_spin_passes * 256, g_effect_projectile_spin_passes * 128);
            }
            battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, parameter);
        } else {
            item = g_effect_current_secondary->used_weapon_id;
            if (item >= ITEM_ID_SHURIKEN && item <= ITEM_ID_YAGYU_DARKNESS) {
                scale.vz = 0xc0;
                scale.vy = 0xc0;
                scale.vx = 0xc0;
                spin.vy = 0;
                spin.vx = 0;
                spin.vz = -(g_effect_projectile_spin_passes * 256);
                battle_effect_draw_projectile_model(&spin, &position, &angles, &scale, EFFECT_MODEL_SHURIKEN);
            } else {
                battle_effect_draw_item_drop_sprite(&position, item, 0);
            }
        }
        if (g_effect_projectile_spin_passes == 2 && g_effect_trajectory_hit_unit_id != -1) {
            kind = g_effect_current_secondary->used_weapon_id;
            if ((kind < ITEM_ID_FIRE_BALL || kind > ITEM_ID_LIGHTNING_BALL)
                && (kind < ITEM_ID_POTION || kind > ITEM_ID_PHOENIX_DOWN)
                && g_effect_current_secondary->target_count != 0) {
                battle_unit_call_bow_hardcoding_by_runtime_id(g_effect_current_secondary->caster.data.unit.runtime_id,
                    g_effect_current_secondary->target.record.data.unit.runtime_id);
            }
        }
        if (g_effect_projectile_spin_passes > 0) {
            g_effect_projectile_spin_passes--;
        }
        if (g_effect_projectile_spin_passes == 0) {
            if (g_effect_current_secondary->slot_ids[0] != 0) {
                battle_effect_free_slot(g_effect_current_secondary->slot_ids[0]);
                g_effect_current_secondary->slot_ids[0] = 0;
            }
            if (g_effect_current_secondary->target.record.target_type == EFFECT_TARGET_TYPE_UNIT
                && g_effect_current_secondary->target.record.data.unit.result_animation != EFFECT_HIT_MISSED
                && g_effect_current_secondary->target.record.data.unit.result_animation != EFFECT_HIT_NO_EFFECT) {
                battle_effect_play();
                battle_state_set_animation_speed(2);
            } else if (battle_effect_get_playback_phase() == EFFECT_PLAYBACK_PHASE_REQUESTED) {
                if (g_effect_current_secondary->target_count != 0) {
                    battle_unit_call_bow_hardcoding_by_runtime_id(
                        g_effect_current_secondary->caster.data.unit.runtime_id,
                        g_effect_current_secondary->target.record.data.unit.runtime_id);
                }
                battle_effect_cancel_playback_request();
            }
            g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_FINALIZING;
        }
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        result = 0;
        break;
    }
    return result;
}

/*
 * Secondary effect function 0x15 (Equip Break) in g_effect_secondary_handlers.
 * Same shape as the venom trap
 * handler, but its particle group is held in a variable, so its spawn_start_frame lookup
 * is an indexed table read. Returns 1 to keep animating.
 */
s32 battle_effect_equip_break_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_secondary_data_t* current;
    vec3s32_t gravity;
    s32 group;
    s32 result;
    s16 finish_timer;

    group = EFFECT_PARTICLE_GROUP_EQUIP_BREAK;
    effect = g_effect_current_secondary;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        result = 1;
        effect->active_count = 0;
        effect->timer = 0;
        effect->finish_timer = 0;
        effect->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        battle_effect_start_group(group, 10);
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        gravity.z = 0;
        gravity.x = 0;
        gravity.y = ONE;
        battle_effect_save_gravity();
        battle_effect_set_gravity(&gravity);
        battle_effect_update_active_slots(group);
        battle_effect_restore_inertia_threshold();
        battle_effect_restore_gravity();
        current = g_effect_current_secondary;
        finish_timer = current->finish_timer;
        result = finish_timer != 1;
        current->timer = current->timer + 1;
        if (current->active_count == 0 && current->timer > g_effect_trap_particle_configs[group].spawn_start_frame) {
            current->finish_timer = finish_timer + 1;
        }
        break;
    }
    return result;
}

enum {
    EFFECT_RING_CLUT = MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 12),
    EFFECT_RING_SLOT_COUNT = 30,
};

/* Secondary effect 0x16 (summon charge): three rings of ten effect slots
 * spinning around the target.
 *
 * Initialization allocates the 30
 * slots and plays sound 0x21; the executing phase grows the radius, spin and
 * brightness with the timer, the finalizing phase widens the rings and fades
 * them out over finish_timer, and both then advance the ring angle, record each
 * ring's newest point and place every slot at a trailing point. Once finish_timer
 * passes 60 the slots are freed and sound 0x21 is paused. Returns 1 while the
 * effect runs.
 */
s32 battle_effect_summon_charge_secondary_handler(void) {
    effect_secondary_data_t* effect;
    effect_ring_state_t* state;
    effect_slot_t* slot;
    s32 unused[6]; /* Reserves the target's 0x68-byte frame. */
    s32 result;
    s32 i;
    s32 j;
    s32 k;
    u8 id;

    effect = g_effect_current_secondary;
    state = &g_effect_secondary_state.summon_ring;
    /* Preserve the target's unsigned phase-dispatch comparison. */
    switch (effect->phase) {
    case EFFECT_SECONDARY_PHASE_INITIALIZING:
        state->index = 0;
        state->radius = 0;
        state->angle_step = 0;
        state->angle = 0;
        battle_effect_resolve_target_world_coords(0, EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &state->center);
        state->brightness = 0;
        state->center.vy -= 24;
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 10; j++) {
                state->points[i][j].vx = state->points[i][j].vy = state->points[i][j].vz = 0;
            }
        }
        for (i = 0; i < EFFECT_RING_SLOT_COUNT; i++) {
            j = g_effect_current_secondary->slot_ids[i] = battle_effect_alloc_slot();
            if (j != 0) {
                slot = &g_effect_slots[j];
                slot->life = battle_effect_spawn_particle_motion(
                    &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SUMMON_CHARGE], &slot->motion);
                battle_effect_init_trap_animation(EFFECT_PARTICLE_GROUP_SUMMON_CHARGE, EFFECT_RING_CLUT, slot);
            }
        }
        g_effect_current_secondary->timer = 0;
        g_effect_current_secondary->finish_timer = 0;
        g_effect_current_secondary->phase = EFFECT_SECONDARY_PHASE_EXECUTING;
        suzuki_sound_play_sfx(MAIN_SFX_SUMMON_CHARGE);
        result = 1;
        break;
    case EFFECT_SECONDARY_PHASE_EXECUTING:
        if (effect->timer < 24) {
            state->radius = effect->timer;
            state->angle_step = effect->timer * 3;
        }
        if (effect->timer < 8) {
            state->brightness = (effect->timer + 1) * 16;
        }
        if (effect->timer < 256) {
            effect->timer++;
        }
        goto update; /* skip the finalizing fade into the shared update */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        if (effect->finish_timer < 61) {
            if (effect->finish_timer & 8) {
                state->radius++;
            }
            state->angle_step++;
            if (effect->finish_timer < 61) {
                state->brightness = (60 - effect->finish_timer) * 4;
                if (effect->finish_timer < 61) {
                    effect->finish_timer++;
                }
            }
        }
    update:
        state->angle += state->angle_step;
        for (i = 0; i < 3; i++) {
            state->points[i][state->index].vx = rcos(i * 0x555 + state->angle) * state->radius / ONE;
            state->points[i][state->index].vz = rsin(i * 0x555 + state->angle) * state->radius / ONE;
        }
        for (i = 0; i < 3; i++) {
            for (j = 0; j < 10; j++) {
                id = g_effect_current_secondary->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_effect_slots[id];
                    slot->color.r = slot->color.g = slot->color.b
                        = g_effect_summon_ring_brightness[j] * state->brightness / 128;
                }
            }
        }
        for (i = 0; i < 3; i++) {
            k = state->index;
            for (j = 0; j < 10; j++) {
                id = g_effect_current_secondary->slot_ids[i * 10 + j];
                if (id != 0) {
                    slot = &g_effect_slots[id];
                    slot->motion.position.x = (state->center.vx + state->points[i][k].vx) * ONE;
                    slot->motion.position.y = (state->center.vy + state->points[i][k].vy) * ONE;
                    slot->motion.position.z = (state->center.vz + state->points[i][k].vz) * ONE;
                    battle_effect_update_slot(g_effect_current_secondary->slot_ids[i * 10 + j]);
                }
                k++;
                if (k == 10) {
                    k = 0;
                }
            }
        }
        state->index++;
        if (state->index >= 10) {
            state->index = 0;
        }
        if (g_effect_current_secondary->finish_timer >= 61) {
            for (i = 0; i < EFFECT_RING_SLOT_COUNT; i++) {
                if (g_effect_current_secondary->slot_ids[i] != 0) {
                    battle_effect_free_slot(g_effect_current_secondary->slot_ids[i]);
                    g_effect_current_secondary->slot_ids[i] = 0;
                }
            }
            suzuki_sound_stop_sfx_channels(MAIN_SFX_SUMMON_CHARGE);
            result = 0;
        } else {
            result = 1;
        }
        break;
    }
    return result;
}

/* Run the handler of every active secondary effect once, newest first, and remove
 * each one whose handler returns 0. Returns 0 when none is active.
 *
 * Before the handlers run it takes this frame's ordering table and flips the
 * buffer index their double-buffered primitives use. */
s32 battle_effect_update_secondary_effects(void) {
    effect_secondary_data_t* effect;
    u32 previous_slot_id;

    g_effect_polygon_depth_data = main_gfx_get_otag();
    g_effect_buffer_index = 1 - g_effect_buffer_index;
    g_effect_current_slot_id = g_effect_secondary_active_tail;
    if (g_effect_secondary_active_tail == 0) {
        return 0;
    }

    do {
        effect = &g_effect_secondary_data[g_effect_current_slot_id];
        g_effect_current_secondary = effect;
        previous_slot_id = effect->previous_slot_id;
        if (g_effect_secondary_handlers[effect->function_id]() == 0) {
            battle_effect_remove_secondary(g_effect_current_slot_id);
        }
        g_effect_current_slot_id = previous_slot_id;
    } while (previous_slot_id != 0);

    return 1;
}

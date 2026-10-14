#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/suzuki.h"

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
s32 effect_spell_charge_secondary_handler(void) {
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
        g_effect_current_secondary->parameter = effect_get_element_index(effect->parameter);
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
                    j = state->trails[i].slot_id = effect_alloc_slot();
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
                state->ids_b[j] = effect_alloc_slot();
                if (state->ids_b[j] == 0) {
                    break;
                }
                slot = &g_effect_slots[state->ids_b[j]];
                slot->life = effect_spawn_particle_motion(
                    &g_effect_trap_particle_configs[EFFECT_PARTICLE_GROUP_SPELL_CHARGE], &slot->motion);
                effect_init_trap_animation(
                    EFFECT_PARTICLE_GROUP_SPELL_CHARGE, MAP_CLUT(MAP_CLUT_ROW_WEAPONS, 15), slot);
                state->count.particles++;
                if (state->count.particles == 14) {
                    break;
                }
            }
        }
        /* fallthrough */
    case EFFECT_SECONDARY_PHASE_FINALIZING:
        effect_save_inertia_threshold();
        effect_set_inertia_threshold(EFFECT_SECONDARY_INERTIA_THRESHOLD);
        for (i = 0; i < 2; i++) {
            j = state->ids_a[i];
            if (j != 0 && effect_update_slot(j) == 0) {
                effect_free_slot(j);
                state->ids_a[i] = 0;
                state->count.auxiliary_particles--;
            }
        }
        for (i = 0; i < 14; i++) {
            j = state->ids_b[i];
            if (j != 0 && effect_update_slot(j) == 0) {
                effect_free_slot(j);
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
                        = effect_interpolate_s32_cos(x >> 12, state->x, state->duration, age);
                    fade = 6;
                    state->trails[i].points[head].vy
                        = effect_interpolate_s32_cos(y >> 12, state->y, state->duration, age);
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
                    effect_free_slot(j);
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

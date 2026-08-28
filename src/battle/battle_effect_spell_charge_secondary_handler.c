#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/effect.h"
#include "fft/main_sound.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/libc.h"

typedef struct battle_effect_charge_params {
    s16 speed;
    s16 spread_range;
    s16 spread_offset;
    s16 duration;
    s16 radius;
    u8 spawn_interval;
    u8 _unknown_0b;
    u8 max_trails;
    u8 fade_row;
} battle_effect_charge_params_t;

/* The target compares a line's two endpoints as whole words. */
#define LINE_G2_XY_WORD(line, n) (*(s32*)&(line)->x##n)

extern battle_effect_charge_params_t g_battle_effect_charge_line_params[];

/* Secondary handler for a charge effect: phase 1 loads the animation's
 * parameters and primes the line primitives, phase 2 spawns trails that
 * converge on the caster, and phases 2-3 advance and draw them.
 *
 * The OT link masks `line` before `setaddr(line, getaddr(ot + 1))`: that keeps
 * the 0xffffff mask's loop lifetime short enough for loop.c to leave it in the
 * inner loop, and getaddr's extra mask reference gives it priority over the
 * 0xff000000 mask in local allocation (a1 versus a2 in the target). */
s32 battle_effect_spell_charge_secondary_handler(void) {
    battle_effect_secondary_data_t* effect;
    battle_effect_charge_state_t* state;
    SVECTOR in;
    s32 unused[4];
    VECTOR out;
    LINE_G2* line;
    battle_screen_coords_t* screen;
    battle_effect_slot_t* slot;
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
    battle_effect_slot_t* work;

    effect = g_battle_effect_current_secondary;
    state = &g_battle_effect_secondary_state.charge;
    switch ((u32)effect->phase) {
    case BATTLE_SECONDARY_EFFECT_INITIALIZING:
        index = g_battle_effect_secondary_animations[effect->animation_id].parameter;
        g_battle_effect_current_secondary->parameter = battle_effect_get_ninja_ball((u16)effect->parameter);
        g_battle_effect_secondary_state.charge.speed = g_battle_effect_charge_line_params[index].speed;
        g_battle_effect_secondary_state.charge.spread_range = g_battle_effect_charge_line_params[index].spread_range;
        g_battle_effect_secondary_state.charge.spread_offset = g_battle_effect_charge_line_params[index].spread_offset;
        g_battle_effect_secondary_state.charge.duration = g_battle_effect_charge_line_params[index].duration;
        g_battle_effect_secondary_state.charge.radius = g_battle_effect_charge_line_params[index].radius;
        g_battle_effect_secondary_state.charge.spawn_interval
            = g_battle_effect_charge_line_params[index].spawn_interval;
        g_battle_effect_secondary_state.charge.colour = (u16)g_battle_effect_current_secondary->parameter;
        g_battle_effect_secondary_state.charge.max_trails = g_battle_effect_charge_line_params[index].max_trails;
        g_battle_effect_secondary_state.charge.fade_row = g_battle_effect_charge_line_params[index].fade_row;
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
        state->count.n.trails = 0;
        state->count.n.a = 0;
        state->count.n.b = 0;
        for (i = 0; i < 2; i++) {
            state->ids_a[i] = 0;
        }
        for (i = 0; i < 14; i++) {
            state->ids_b[i] = 0;
        }
        screen
            = battle_unit_get_screen_data_ptr_by_misc_id((u8)g_battle_effect_current_secondary->caster_block.values[1]);
        i = battle_gfx_get_unit_spritesheet_height_by_misc_id(
                (u8)g_battle_effect_current_secondary->caster_block.values[1])
            + 8;
        g_battle_effect_groups[12].position_y = g_battle_effect_groups[13].position_y = -i;
        in.vx = screen->x;
        in.vz = screen->y;
        in.vy = screen->z - i;
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        RotTrans(&in, &out, (long*)&out.pad);
        state->angle = 0;
        state->done = 0;
        state->x = out.vx;
        state->y = out.vy;
        SuzukiPlaySoundFindChannel(0x1f);
        result = 1;
        g_battle_effect_current_secondary->phase = BATTLE_SECONDARY_EFFECT_EXECUTING;
        break;
    case BATTLE_SECONDARY_EFFECT_EXECUTING:
        if (g_battle_effect_secondary_state.charge.done == 0
            && g_battle_effect_secondary_state.charge.count.n.trails < g_battle_effect_secondary_state.charge.max_trails
            && rand() % g_battle_effect_secondary_state.charge.spawn_interval == 0) {
            for (i = 0; i < 16; i++) {
                if (state->trails[i].slot_id == 0) {
                    j = state->trails[i].slot_id = battle_effect_alloc_slot();
                    if (j == 0) {
                        break;
                    }
                    state->count.n.trails++;
                    k = (rand() & 0x1ff) + state->angle;
                    state->angle += 0x571;
                    g_battle_effect_slots[j].motion.position[0] = rcos(k) * state->radius + (state->x << 12);
                    g_battle_effect_slots[j].motion.position[1] = rsin(k) * state->radius + (state->y << 12);
                    k = k + rand() % state->spread_range - state->spread_offset - 0x800;
                    g_battle_effect_slots[j].motion.velocity[0] = rcos(k) * state->speed;
                    g_battle_effect_slots[j].motion.velocity[1] = rsin(k) * state->speed;
                    g_battle_effect_slots[j].life = 1;
                    for (k = 0; k < 7; k++) {
                        state->trails[i].points[k].vx = g_battle_effect_slots[j].motion.position[0] >> 12;
                        state->trails[i].points[k].vy = g_battle_effect_slots[j].motion.position[1] >> 12;
                    }
                    state->trails[i].age = 0;
                    break;
                }
            }
        }
        if (state->count.n.b < 14) {
            for (i = 0; i < g_battle_effect_groups[12].spawns_per_frame; i++) {
                for (j = 0; j < 14; j++) {
                    if (state->ids_b[j] == 0) {
                        break;
                    }
                }
                state->ids_b[j] = battle_effect_alloc_slot();
                if (state->ids_b[j] == 0) {
                    break;
                }
                slot = &g_battle_effect_slots[state->ids_b[j]];
                slot->life = battle_effect_spawn_particle_motion(
                    (battle_effect_emitter_values_t*)((u8*)&g_battle_effect_groups[12] - 2), &slot->motion);
                battle_effect_init_trap_animation(0xc, 0x7acf, slot);
                state->count.n.b++;
                if (state->count.n.b == 14) {
                    break;
                }
            }
        }
        /* fallthrough */
    case BATTLE_SECONDARY_EFFECT_FINALIZING:
        battle_effect_save_inertia_threshold();
        battle_effect_set_inertia_threshold(0x230);
        for (i = 0; i < 2; i++) {
            j = state->ids_a[i];
            if (j != 0 && battle_effect_update_slot(j) == 0) {
                battle_effect_free_slot(j);
                state->ids_a[i] = 0;
                state->count.n.a--;
            }
        }
        for (i = 0; i < 14; i++) {
            j = state->ids_b[i];
            if (j != 0 && battle_effect_update_slot(j) == 0) {
                battle_effect_free_slot(j);
                state->ids_b[i] = 0;
                state->count.n.b--;
            }
        }
        red = g_battle_effect_trap_colors[state->colour][0];
        green = g_battle_effect_trap_colors[state->colour][1];
        blue = g_battle_effect_trap_colors[state->colour][2];
        offsets = &g_battle_effect_charge_line_direction_offsets[0].vx;
        offsets_y = &g_battle_effect_charge_line_direction_offsets[0].vy;
        for (i = 0; i < 16; i++) {
            j = state->trails[i].slot_id;
            if (j != 0) {
                work = &g_battle_effect_slots[j];
                work->motion.position[0] = x
                    = g_battle_effect_slots[j].motion.position[0] + g_battle_effect_slots[j].motion.velocity[0];
                work->motion.position[1] = y
                    = g_battle_effect_slots[j].motion.position[1] + g_battle_effect_slots[j].motion.velocity[1];
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
                    line = &state->lines[g_battle_effect_buffer_index][i * 6 + k];
                    line->x0 = state->trails[i].points[head].vx;
                    line->y0 = state->trails[i].points[head].vy;
                    line->r0 = red * g_battle_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8;
                    line->g0 = green * g_battle_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8;
                    line->b0 = blue * g_battle_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8;
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
                    line->r1 = red * g_battle_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8;
                    line->g1 = green * g_battle_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8;
                    line->b1 = blue * g_battle_effect_charge_line_fade_levels[state->fade_row * 7 + fade] >> 8;
                    if (LINE_G2_XY_WORD(line, 0) != LINE_G2_XY_WORD(line, 1)) {
                        dir = (ratan2(line->y1 - line->y0, line->x1 - line->x0) + 0x900) / 512;
                        line->x1 += offsets[dir * 2];
                        line->y1 += offsets_y[dir * 2];
                        ot = g_battle_effect_polygon_depth_data;
                        link = (u32)line & 0xffffff;
                        setaddr(line, getaddr(ot + 1));
                        ot[1] = (ot[1] & 0xff000000) | link;
                    }
                }
                if (state->trails[i].age >= state->duration + 7) {
                    battle_effect_free_slot(j);
                    state->trails[i].slot_id = 0;
                    state->count.n.trails--;
                }
            }
        }
        AddPrim((u32*)g_battle_effect_polygon_depth_data + 1, &state->mode[g_battle_effect_buffer_index]);
        if (g_battle_effect_current_secondary->phase == BATTLE_SECONDARY_EFFECT_EXECUTING || state->count.all != 0) {
            result = 1;
        } else {
            main_sound_stop_sfx_channels(0x1f);
            result = 0;
        }
        break;
    }
    return result;
}

#include "fft/battle.h"
#include "psx/types.h"

/* Animate-tick state overlaid on effect_record_t from 0x26. */
typedef struct battle_effect_tick_state {
    u16 target_index;          /* 0x00 (record 0x26) */
    s16 frame;                 /* 0x02 */
    s16 particle_keyframe[5];  /* 0x04 */
    s16 sound_keyframe[3];     /* 0x0e */
    s16 color_keyframe[4];     /* 0x14 */
    u8 _unknown_1c[2];         /* 0x1c */
    s16 particle_remaining[5]; /* 0x1e */
    s16 sound_remaining[3];    /* 0x28 */
    s16 color_remaining[4];    /* 0x2e */
    u8 _unknown_36[2];         /* 0x36 */
    u16 particle_step[5];      /* 0x38 */
} battle_effect_tick_state_t;

typedef struct battle_effect_tick_record_view {
    u8 _unknown_00[0x28];
    u16 frame; /* 0x28 */
} battle_effect_tick_record_view_t;

/* Advance a single-phase effect timeline by one frame (script opcode 0x28).
 *
 * The five particle channels dispatch keyframe actions and spawn emitters,
 * the four colour tracks recolour the affected units, the caster, the current
 * target and the screen, and the three sound channels play on-hit sounds; a
 * root effect then moves the camera and, under effect flag 0x40, follows the
 * timing curve. A target whose record byte 0x03 is 2 instead starts the
 * secondary effect 10 and jumps to the timeline's end frame.
 *
 * The body inlines the logic of battle_effect_step_emitter_timeline and the
 * palette/background/sound track helpers. Each track keeps its own keyframe
 * local and the timers reuse i, as in those helpers: both reproduce the
 * target's load sharing and register allocation. */
s32 battle_effect_code_script_28_step_child_timeline(effect_record_t* record) {
    map_background_gradient_colors_t colors;
    battle_effect_secondary_init_t init;
    battle_effect_tick_state_t* state;
    battle_effect_keyframe_table_t* table;
    battle_effect_palette_track_t* track;
    battle_effect_background_track_t* screen;
    battle_effect_tick_sound_track_t* sound;
    s32 i;
    s16 k;
    s16 units_keyframe;
    s16 caster_keyframe;
    s16 target_keyframe;
    s16 screen_keyframe;
    s16 sound_keyframe;
    s32 index;
    s32 preset;
    s32 sel;
    u16* selectors;
    u16 slot;
    s16 step;
    s16 red;
    s16 green;
    s16 blue;
    u8 target;
    s32 mode;
    s32 kind;
    s32 sound_index;

    target = record->target_index;
    state = (battle_effect_tick_state_t*)&record->target_index;
    if (g_battle_effect_targets[target].id.bytes[1] != 2) {
        for (i = 0; i < 5; i++) {
            table = &g_battle_effect_timing_channels->particle[i];
            if (state->particle_remaining[i] == 0) {
                if (state->particle_keyframe[i] == 0) {
                    state->particle_remaining[i] = table->frame_start[1] - table->frame_start[0];
                    state->particle_step[i] = 0;
                    state->particle_keyframe[i]++;
                    battle_effect_run_keyframe_actions(
                        table->selector[1], target, (battle_keyframe_effect_state_t*)record);
                }
                while (state->particle_remaining[i] == 0) {
                    k = state->particle_keyframe[i];
                    state->particle_keyframe[i] = k + 1;
                    state->particle_remaining[i] = table->frame_start[k + 1] - table->frame_start[k];
                    state->particle_step[i] = 0;
                    battle_effect_run_keyframe_actions(
                        table->selector[k + 1], target, (battle_keyframe_effect_state_t*)record);
                }
            }
            index = table->action[state->particle_keyframe[i]];
            if (index != 0) {
                step = state->particle_step[i]++;
                selectors = table->selector;
                sel = selectors[state->particle_keyframe[i]] & 7;
                if (sel != 0) {
                    slot = sel - 1;
                    if (state->particle_remaining[i] == 2) {
                        record->phase[slot] = EFFECT_PHASE_DESTROY;
                    }
                    ((void (*)(s16, s32, s32, s16))record->values_d4[slot])(
                        record->record_index, slot, index - 1, step);
                } else {
                    battle_effect_spawn_emitter_particles(record->record_index, step, index - 1, 0);
                }
            }
            state->particle_remaining[i]--;
        }

        track = &g_battle_effect_timing_channels->affected_units;
        units_keyframe = state->color_keyframe[0];
        if (units_keyframe < track->count - 1) {
            if (state->color_remaining[0] == 0) {
                state->color_keyframe[0] = units_keyframe + 1;
                index = units_keyframe;
                i = track->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                state->color_remaining[0] = i;
                preset = track->preset[index];
                if (preset & 0x80) {
                    red = track->color[index][0];
                    green = track->color[index][1];
                    blue = track->color[index][2];
                    battle_map_color_field(preset & 0x7f, track->duration[index], red, green, blue);
                    for (i = 0; i < g_battle_effect_coord_data.palette_target_count; i++) {
                        battle_gfx_start_misc_unit_palette_modulation(preset & 0x7f, track->duration[index],
                            g_battle_effect_palette_target_misc_ids[i], red, green, blue);
                    }
                    if (!(g_battle_map_weather_flags & 2)) {
                        battle_map_start_darkness_blend(
                            preset & 0x7f, track->duration[index], red * 8, green * 8, blue * 8);
                    }
                }
            }
            state->color_remaining[0]--;
        }

        track = &g_battle_effect_timing_channels->caster;
        caster_keyframe = state->color_keyframe[1];
        if (caster_keyframe < track->count - 1) {
            if (state->color_remaining[1] == 0) {
                state->color_keyframe[1] = caster_keyframe + 1;
                index = caster_keyframe;
                i = track->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                state->color_remaining[1] = i;
                preset = track->preset[index];
                if (preset & 0x80) {
                    red = track->color[index][0];
                    green = track->color[index][1];
                    blue = track->color[index][2];
                    if (g_battle_effect_targets[16].target_type == 0) {
                        battle_gfx_start_misc_unit_palette_modulation(preset & 0x7f, track->duration[index],
                            g_battle_effect_targets[16].id.misc_id, red, green, blue);
                    }
                }
            }
            state->color_remaining[1]--;
        }

        track = &g_battle_effect_timing_channels->target;
        target_keyframe = state->color_keyframe[2];
        if (target_keyframe < track->count - 1) {
            if (state->color_remaining[2] == 0) {
                state->color_keyframe[2] = target_keyframe + 1;
                index = target_keyframe;
                i = track->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                state->color_remaining[2] = i;
                preset = track->preset[index];
                if (preset & 0x80) {
                    red = track->color[index][0];
                    green = track->color[index][1];
                    blue = track->color[index][2];
                    if (g_battle_effect_targets[target].target_type == 0) {
                        battle_gfx_start_misc_unit_palette_modulation(preset & 0x7f, track->duration[index],
                            g_battle_effect_targets[target].id.misc_id, red, green, blue);
                    }
                }
            }
            state->color_remaining[2]--;
        }

        screen = &g_battle_effect_timing_channels->screen;
        screen_keyframe = state->color_keyframe[3];
        if (screen_keyframe < screen->count - 1) {
            if (state->color_remaining[3] == 0) {
                state->color_keyframe[3] = screen_keyframe + 1;
                index = screen_keyframe;
                i = screen->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                state->color_remaining[3] = i;
                preset = screen->preset[index];
                mode = preset & 0x7f;
                if (preset & 0x80) {
                    red = screen->color[index][0] * 2;
                    green = screen->color[index][1] * 2;
                    blue = screen->color[index][2] * 2;
                    battle_map_modify_background_gradient(mode, screen->duration[index], red, green, blue);
                    if (g_battle_map_weather_flags & 2) {
                        battle_map_start_darkness_blend(mode, screen->duration[index], red, green, blue);
                    }
                } else {
                    colors.first.red = screen->color[index][0];
                    colors.first.green = screen->color[index][1];
                    colors.first.blue = screen->color[index][2];
                    colors.second.red = screen->second_color[index][0];
                    colors.second.green = screen->second_color[index][1];
                    colors.second.blue = screen->second_color[index][2];
                    battle_map_set_background_gradient(screen->duration[index], &colors);
                    if (g_battle_map_weather_flags & 2) {
                        battle_map_set_darkness_color(screen->duration[index], &colors.first);
                    }
                }
            }
            state->color_remaining[3]--;
        }

        for (i = 0; i < 3; i++) {
            sound = &g_battle_effect_timing_channels->sound[i];
            sound_keyframe = state->sound_keyframe[i];
            if (sound_keyframe < sound->count) {
                if (state->sound_remaining[i] == 0) {
                    state->sound_keyframe[i] = sound_keyframe + 1;
                    index = sound_keyframe;
                    state->sound_remaining[i] = sound->duration[index];
                    kind = sound->kind[index];
                    if (kind >= 2) {
                        sound_index = battle_effect_get_on_hit_sound_index(kind - 2);
                        if (sound_index != 0) {
                            SuzukiPlaySoundFindChannel(sound_index + g_battle_effect_sound_data_base);
                        }
                    }
                }
                state->sound_remaining[i]--;
            }
        }
        if (record->parent_index == 0) {
            battle_camera_start_effect_keyframe_moves(state->frame);
        }
        if (g_battle_effect_flags_section->flags & EFFECT_FLAG_TIMING_CURVE_PHASE2) {
            battle_state_set_time_scale(
                battle_effect_get_animation_timing_curve_value((battle_effect_timing_entry_t*)record));
        }
        state->frame++;
    } else {
        if (g_battle_effect_targets[target].target_type == 0) {
            init.caster.block = *(battle_effect_secondary_block_t*)&g_battle_effect_targets[16];
            init.target.block = *(battle_effect_secondary_block_t*)&g_battle_effect_targets[target];
            battle_effect_init_secondary(10, g_battle_effect_misc_data->spawn_delay, &init);
        }
        ((battle_effect_tick_record_view_t*)record)->frame = g_battle_effect_timing_channels->duration;
    }
    record->pc += 2;
    return 1;
}

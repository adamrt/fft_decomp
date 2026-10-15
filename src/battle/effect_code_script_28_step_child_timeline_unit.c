#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Opcode 28: advance the record's child timeline by one frame, for the
 * record's target.
 *
 * The five particle channels run keyframe actions and spawn emitters, the
 * four color tracks recolor the other units, the caster, the target and the
 * screen, and the three sound tracks play the effect's sounds; a root record
 * then starts the child camera table's moves and, under
 * EFFECT_FLAG_TIMING_CURVE_CHILD, the time scale follows the animation
 * timing curve. A target that reflected the effect instead starts the
 * reflect secondary effect, lasting spawn_delay frames, and jumps the
 * timeline to its end.
 *
 * The body inlines the logic of effect_step_emitter_timeline and the
 * palette/background/sound track helpers. Each track keeps its own keyframe
 * local and the timers reuse i, as in those helpers: both reproduce the
 * target's load sharing and register allocation. */
effect_script_result_e effect_code_script_28_step_child_timeline(effect_record_t* record) {
    map_background_gradient_colors_t colors;
    effect_secondary_init_t init;
    effect_record_timeline_t* timeline;
    effect_keyframe_table_t* table;
    effect_palette_track_t* track;
    effect_background_track_t* screen;
    effect_child_sound_track_t* sound;
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
    u16* actions;
    u16 slot;
    s16 step;
    s16 red;
    s16 green;
    s16 blue;
    u8 target;
    s32 mode;
    s32 kind;
    s32 sound_index;

    target = record->timeline.target_index;
    timeline = &record->timeline;
    if (g_effect_context.targets[target].data.unit.result_animation != EFFECT_HIT_REFLECTED) {
        for (i = 0; i < EFFECT_PARTICLE_CHANNEL_COUNT; i++) {
            table = &g_effect_child_timeline->particle[i];
            if (timeline->state.child.remaining.tracks.particle[i] == 0) {
                if (timeline->state.child.keyframe.tracks.particle[i] == 0) {
                    timeline->state.child.remaining.tracks.particle[i] = table->frame_start[1] - table->frame_start[0];
                    timeline->state.child.step.tracks.particle[i] = 0;
                    timeline->state.child.keyframe.tracks.particle[i]++;
                    effect_run_keyframe_actions(table->action[1], target, record);
                }
                while (timeline->state.child.remaining.tracks.particle[i] == 0) {
                    k = timeline->state.child.keyframe.tracks.particle[i];
                    timeline->state.child.keyframe.tracks.particle[i] = k + 1;
                    timeline->state.child.remaining.tracks.particle[i]
                        = table->frame_start[k + 1] - table->frame_start[k];
                    timeline->state.child.step.tracks.particle[i] = 0;
                    effect_run_keyframe_actions(table->action[k + 1], target, record);
                }
            }
            index = table->emitter[timeline->state.child.keyframe.tracks.particle[i] - 1];
            if (index != 0) {
                step = timeline->state.child.step.tracks.particle[i]++;
                actions = table->action;
                sel = actions[timeline->state.child.keyframe.tracks.particle[i]] & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK;
                if (sel != 0) {
                    slot = sel - 1;
                    if (timeline->state.child.remaining.tracks.particle[i] == 2) {
                        record->callback_phase[slot] = EFFECT_CALLBACK_PHASE_DESTROY;
                    }
                    record->callbacks[slot](record->record_index, slot, index - 1, step);
                } else {
                    effect_spawn_emitter_particles(record->record_index, step, index - 1, 0);
                }
            }
            timeline->state.child.remaining.tracks.particle[i]--;
        }

        track = &g_effect_child_timeline->surroundings;
        units_keyframe = timeline->state.child.keyframe.tracks.surroundings;
        if (units_keyframe < track->count - 1) {
            if (timeline->state.child.remaining.tracks.surroundings == 0) {
                timeline->state.child.keyframe.tracks.surroundings = units_keyframe + 1;
                index = units_keyframe;
                i = track->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                timeline->state.child.remaining.tracks.surroundings = i;
                preset = track->preset[index];
                if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                    red = track->color[index].r;
                    green = track->color[index].g;
                    blue = track->color[index].b;
                    map_color_field(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK, track->duration[index], red, green, blue);
                    for (i = 0; i < g_effect_context.spectator_count; i++) {
                        battle_gfx_apply_unit_palette_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                            track->duration[index], g_effect_palette_target_runtime_ids[i], red, green, blue);
                    }
                    if (!(g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER)) {
                        map_start_darkness_blend(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK, track->duration[index],
                            red * 8, green * 8, blue * 8);
                    }
                }
            }
            timeline->state.child.remaining.tracks.surroundings--;
        }

        track = &g_effect_child_timeline->caster;
        caster_keyframe = timeline->state.child.keyframe.tracks.caster;
        if (caster_keyframe < track->count - 1) {
            if (timeline->state.child.remaining.tracks.caster == 0) {
                timeline->state.child.keyframe.tracks.caster = caster_keyframe + 1;
                index = caster_keyframe;
                i = track->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                timeline->state.child.remaining.tracks.caster = i;
                preset = track->preset[index];
                if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                    red = track->color[index].r;
                    green = track->color[index].g;
                    blue = track->color[index].b;
                    if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
                        battle_gfx_apply_unit_palette_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                            track->duration[index], g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id,
                            red, green, blue);
                    }
                }
            }
            timeline->state.child.remaining.tracks.caster--;
        }

        track = &g_effect_child_timeline->target;
        target_keyframe = timeline->state.child.keyframe.tracks.target;
        if (target_keyframe < track->count - 1) {
            if (timeline->state.child.remaining.tracks.target == 0) {
                timeline->state.child.keyframe.tracks.target = target_keyframe + 1;
                index = target_keyframe;
                i = track->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                timeline->state.child.remaining.tracks.target = i;
                preset = track->preset[index];
                if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                    red = track->color[index].r;
                    green = track->color[index].g;
                    blue = track->color[index].b;
                    if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
                        battle_gfx_apply_unit_palette_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                            track->duration[index], g_effect_context.targets[target].data.unit.runtime_id, red, green,
                            blue);
                    }
                }
            }
            timeline->state.child.remaining.tracks.target--;
        }

        screen = &g_effect_child_timeline->screen;
        screen_keyframe = timeline->state.child.keyframe.tracks.screen;
        if (screen_keyframe < screen->count - 1) {
            if (timeline->state.child.remaining.tracks.screen == 0) {
                timeline->state.child.keyframe.tracks.screen = screen_keyframe + 1;
                index = screen_keyframe;
                i = screen->duration[index];
                if (i == 0) {
                    i = 1;
                } else {
                    i *= 8;
                }
                timeline->state.child.remaining.tracks.screen = i;
                preset = screen->preset[index];
                mode = preset & EFFECT_COLOR_KEYFRAME_MODE_MASK;
                if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                    red = screen->color[index].r * 2;
                    green = screen->color[index].g * 2;
                    blue = screen->color[index].b * 2;
                    map_modify_background_gradient(mode, screen->duration[index], red, green, blue);
                    if (g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) {
                        map_start_darkness_blend(mode, screen->duration[index], red, green, blue);
                    }
                } else {
                    colors.colors[MAP_BACKGROUND_GRADIENT_TOP].r = screen->color[index].r;
                    colors.colors[MAP_BACKGROUND_GRADIENT_TOP].g = screen->color[index].g;
                    colors.colors[MAP_BACKGROUND_GRADIENT_TOP].b = screen->color[index].b;
                    colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r = screen->second_color[index].r;
                    colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g = screen->second_color[index].g;
                    colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b = screen->second_color[index].b;
                    map_set_background_gradient(screen->duration[index], &colors);
                    if (g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) {
                        map_set_darkness_color(screen->duration[index], &colors.colors[MAP_BACKGROUND_GRADIENT_TOP]);
                    }
                }
            }
            timeline->state.child.remaining.tracks.screen--;
        }

        for (i = 0; i < EFFECT_SOUND_TRACK_COUNT; i++) {
            sound = &g_effect_child_timeline->sound[i];
            sound_keyframe = timeline->state.child.keyframe.tracks.sound[i];
            if (sound_keyframe < sound->count) {
                if (timeline->state.child.remaining.tracks.sound[i] == 0) {
                    timeline->state.child.keyframe.tracks.sound[i] = sound_keyframe + 1;
                    index = sound_keyframe;
                    timeline->state.child.remaining.tracks.sound[i] = sound->duration[index];
                    kind = sound->channel[index];
                    if (kind >= 2) {
                        sound_index = effect_select_sound_id(kind - 2);
                        if (sound_index != 0) {
                            suzuki_sound_play_sfx(sound_index + g_effect_sound_data_base);
                        }
                    }
                }
                timeline->state.child.remaining.tracks.sound[i]--;
            }
        }
        if (record->parent_index == 0) {
            camera_start_effect_keyframe_moves(timeline->frame);
        }
        if (g_effect_flags_section->flags & EFFECT_FLAG_TIMING_CURVE_CHILD) {
            battle_state_set_time_scale(effect_get_animation_timing_curve_value(record));
        }
        timeline->frame++;
    } else {
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            init.targets[EFFECT_CASTER_ENTRY] = g_effect_context.targets[EFFECT_CASTER_ENTRY];
            init.targets[0] = g_effect_context.targets[target];
            effect_init_secondary(EFFECT_SECONDARY_ANIMATION_REFLECT, g_effect_timeline->spawn_delay, &init);
        }
        record->timeline.frame = g_effect_child_timeline->duration;
    }
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/*
 * Dispatch one keyframe's action flags to every live effect target: bits 4-6
 * and 8-15 trigger unit reactions, and bit 3 restricts each of them to the
 * caster entry instead of the whole target list; bits 0-2 register a callback
 * slot.
 */
void effect_run_keyframe_actions_all_targets(u16 flags, effect_record_t* state) {
    s32 runtime_id;
    u16 animation;
    u16 slot;
    s32 i;

    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
        runtime_id = -1;
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            runtime_id = g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id;
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                battle_gfx_prepare_post_action_display_by_runtime_id(runtime_id);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    battle_gfx_prepare_post_action_display_by_runtime_id(
                        g_effect_context.targets[i].data.unit.runtime_id);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                battle_unit_update_display_by_runtime_id(runtime_id);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    battle_unit_update_display_by_runtime_id(g_effect_context.targets[i].data.unit.runtime_id);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                battle_unit_set_target_animation_by_runtime_id(runtime_id);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    battle_unit_set_target_animation_by_runtime_id(g_effect_context.targets[i].data.unit.runtime_id);
                }
            }
        }
    }
    animation = (flags & EFFECT_KEYFRAME_ACTION_ANIMATION_MASK) >> EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT;
    if (animation != 0) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                battle_unit_set_specific_animation_by_runtime_id(runtime_id, animation);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    battle_unit_set_specific_animation_by_runtime_id(
                        g_effect_context.targets[i].data.unit.runtime_id, animation);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) {
        slot = (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) - 1;
        state->callback_phase[slot] = EFFECT_CALLBACK_PHASE_INIT;
        state->callbacks[slot] = g_effect_callback_slots[slot];
    }
}

/* Advances one particle channel of the parent timeline by a frame: enters the
 * keyframes that have come due, running each one's 16-bit action value for every
 * effect target, then spawns the current keyframe's emitter, through the
 * handler of the action's callback slot when it names one. The handler is
 * told to destroy its work on the call made with two frames of the keyframe
 * left. index, remaining and step are the channel's keyframe, the frames left
 * in it and the frames it has run. */
void effect_step_emitter_timeline(
    effect_record_t* state, effect_keyframe_table_t* table, s16* index, s16* remaining, s16* step) {
    s16 k;
    s32 sel;
    u16* actions;
    u16 slot;
    s32 emitter;

    if (table->last_index >= *index) {
        while (*remaining == 0) {
            k = *index;
            *index = k + 1;
            /* Integer-first pointer addition preserves the native shared-address operands. */
            effect_run_keyframe_actions_all_targets(*(k + 1 + table->action), state);
            *remaining = table->frame_start[k + 1] - table->frame_start[k];
            *step = 0;
            if (table->last_index < *index) {
                return;
            }
        }
        if (table->last_index >= *index) {
            emitter = table->emitter[*index - 1];
            if (emitter != 0) {
                actions = table->action;
                sel = actions[*index] & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK;
                slot = sel - 1;
                if (sel != 0) {
                    if (*remaining == 2) {
                        state->callback_phase[slot] = EFFECT_CALLBACK_PHASE_DESTROY;
                    }
                    state->callbacks[slot](state->record_index, slot, emitter - 1, *step);
                } else {
                    effect_spawn_emitter_particles(state->record_index, *step, emitter - 1, 0);
                }
                *step = *step + 1;
            }
            *remaining = *remaining - 1;
        }
    }
}

/* Advance the surroundings color track one frame.
 *
 * When an APPLY keyframe starts, its color is applied to the field, to the
 * palette of every spectator unit (those that are neither a target nor the
 * caster) and, unless weather flag 2 is set, to the scaled map palette. The
 * loop counter reuses the frame-count variable, which keeps both in $s0 as in
 * the target. */
void effect_advance_target_palette_track(effect_palette_track_t* track, s16* keyframe, s16* remaining_frames) {
    s16 current;
    s32 index;
    s32 i;
    s32 preset;
    s16 red;
    s16 green;
    s16 blue;

    current = *keyframe;
    if (current < track->count - 1) {
        if (*remaining_frames == 0) {
            *keyframe = current + 1;
            index = current;
            i = track->duration[index];
            if (i != 0) {
                i *= 8;
            } else {
                i = 1;
            }
            *remaining_frames = i;
            preset = track->preset[index];
            if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                red = track->color[index].r;
                green = track->color[index].g;
                blue = track->color[index].b;
                map_color_field(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK, track->duration[index], red, green, blue);
                for (i = 0; i < g_effect_context.spectator_count; i++) {
                    battle_gfx_apply_unit_palette_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                        track->duration[index], g_effect_palette_target_runtime_ids[i], red, green, blue);
                }
                if (!(g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER)) {
                    map_start_darkness_blend(
                        preset & EFFECT_COLOR_KEYFRAME_MODE_MASK, track->duration[index], red * 8, green * 8, blue * 8);
                }
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}

/* Advance the caster color track one frame: an APPLY keyframe recolors the
 * caster. */
void effect_advance_caster_palette_track(
    effect_palette_track_t* track, s16* keyframe, s16* remaining_frames, s32 caster_runtime_id) {
    s16 current;
    s32 index;
    s32 timer;
    s32 preset;
    s8 red;
    s8 green;
    s8 blue;

    current = *keyframe;
    if (current < track->count - 1) {
        if (*remaining_frames == 0) {
            *keyframe = current + 1;
            index = current;
            timer = track->duration[index];
            if (timer != 0) {
                timer *= 8;
            } else {
                timer = 1;
            }
            *remaining_frames = timer;
            preset = track->preset[index];
            if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                red = track->color[index].r;
                green = track->color[index].g;
                blue = track->color[index].b;
                battle_gfx_apply_unit_palette_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                    track->duration[index], caster_runtime_id, red, green, blue);
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}

/* Advance the target color track of the parent timeline one frame: an APPLY
 * keyframe recolors every unit target of the effect.
 *
 * The frame timer and the loop counter are separate locals here: the target
 * keeps the timer in $v0 and the counter in $s1. */
void effect_advance_coord_target_palette_track(effect_palette_track_t* track, s16* keyframe, s16* remaining_frames) {
    s16 current;
    s32 index;
    s32 i;
    s32 timer;
    s32 preset;
    s16 red;
    s16 green;
    s16 blue;

    current = *keyframe;
    if (current < track->count - 1) {
        if (*remaining_frames == 0) {
            *keyframe = current + 1;
            index = current;
            timer = track->duration[index];
            if (timer != 0) {
                timer *= 8;
            } else {
                timer = 1;
            }
            *remaining_frames = timer;
            preset = track->preset[index];
            if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                red = track->color[index].r;
                green = track->color[index].g;
                blue = track->color[index].b;
                for (i = 0; i < g_effect_context.target_count; i++) {
                    if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                        battle_gfx_apply_unit_palette_modulation_to_units(preset & EFFECT_COLOR_KEYFRAME_MODE_MASK,
                            track->duration[index], g_effect_context.targets[i].data.unit.runtime_id, red, green, blue);
                    }
                }
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}

/* Advance the screen color track one frame.
 *
 * A starting APPLY keyframe modulates the background gradient by
 * its doubled color (and the scaled palette when weather flag 2 is set);
 * otherwise both colors become the new gradient stops, with the first also
 * driving the darkness color under weather flag 2. */
void effect_advance_map_background_track(effect_background_track_t* track, s16* keyframe, s16* remaining_frames) {
    map_background_gradient_colors_t colors;
    s16 current;
    s32 index;
    s16 duration;
    u8 preset;
    s32 mode;
    s32 red;
    s32 green;
    s32 blue;

    current = *keyframe;
    if (current < track->count - 1) {
        if (*remaining_frames == 0) {
            *keyframe = current + 1;
            index = current;
            duration = track->duration[index];
            if (duration != 0) {
                duration *= 8;
            } else {
                duration = 1;
            }
            *remaining_frames = duration;
            preset = track->preset[index];
            if (preset & EFFECT_COLOR_KEYFRAME_APPLY) {
                mode = preset & EFFECT_COLOR_KEYFRAME_MODE_MASK;
                red = track->color[index].r * 2;
                green = track->color[index].g * 2;
                blue = track->color[index].b * 2;
                map_modify_background_gradient(mode, track->duration[index], red, green, blue);
                if (g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) {
                    map_start_darkness_blend(mode, track->duration[index], red, green, blue);
                }
            } else {
                colors.colors[MAP_BACKGROUND_GRADIENT_TOP].r = track->color[index].r;
                colors.colors[MAP_BACKGROUND_GRADIENT_TOP].g = track->color[index].g;
                colors.colors[MAP_BACKGROUND_GRADIENT_TOP].b = track->color[index].b;
                colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].r = track->second_color[index].r;
                colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].g = track->second_color[index].g;
                colors.colors[MAP_BACKGROUND_GRADIENT_BOTTOM].b = track->second_color[index].b;
                map_set_background_gradient(track->duration[index], &colors);
                if (g_map_weather_flags & MAP_WEATHER_FLAG_IGNORE_WEATHER) {
                    map_set_darkness_color(track->duration[index], &colors.colors[MAP_BACKGROUND_GRADIENT_TOP]);
                }
            }
        }
        *remaining_frames = *remaining_frames - 1;
    }
}

/*
 * Advances one sound track of the parent timeline by a frame. The 0x1e-byte
 * track holds nine 16-bit durations at 0x00, nine channel bytes at 0x12
 * (0 and 1 none, otherwise a sound channel of the flags section + 2) and its
 * entry count at 0x1c; entering an entry plays the channel's next sound.
 * Sound ids are relative to g_effect_sound_data_base, the effect sound
 * section's id set by effect_load_sound_section.
 */
void effect_advance_parent_sound_track(
    effect_sound_track_t* schedule, s16* entry_index, s16* countdown, u8* unused_sound_state) {
    s16 index = *entry_index;
    s32 effect_kind;
    s32 sound_index;

    if (index < schedule->entry_count) {
        if (*countdown == 0) {
            *entry_index = index + 1;
            *countdown = schedule->durations[index];
            effect_kind = schedule->channels[index];

            if (effect_kind >= 2) {
                sound_index = effect_select_sound_id(effect_kind - 2);

                if (sound_index != 0) {
                    suzuki_sound_play_sfx(g_effect_sound_data_base + sound_index);
                }
            }
        }
        *countdown -= 1;
    }
}

/* Opcode 29: advance the record's parent timeline by one frame. The second
 * 16-bit value is the script offset its child records start at.
 *
 * Before phase1_duration the phase 1 particle channels, color tracks and
 * sound tracks run (and the time scale follows the timing curve under
 * EFFECT_FLAG_TIMING_CURVE_PHASE1); afterwards one child record per effect
 * target is started, spawn_delay frames apart. Phase 2 runs from
 * phase1_duration + (target_count - 1) * spawn_delay + child_duration. The
 * camera tracks advance every frame. */
effect_script_result_e effect_code_script_29_step_parent_timeline(effect_record_t* record) {
    effect_record_timeline_t* timeline;
    s32 i;
    s16 child;
    u8 unused_stack[16]; /* Unread locals that size the target's 0x48-byte frame. */

    timeline = &record->timeline;
    if (timeline->frame >= g_effect_timeline->phase1_duration) {
        while (timeline->state.parent.spawn_countdown == 0
            && timeline->state.parent.spawned_count < g_effect_context.target_count) {
            const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

            child = effect_start_script_record(record->script, instruction->immediate.value, record->record_index);
            g_effect_state_records[child].timeline.target_index = timeline->state.parent.spawned_count;
            timeline->state.parent.spawned_count++;
            timeline->state.parent.spawn_countdown += g_effect_timeline->spawn_delay;
        }
        timeline->state.parent.spawn_countdown--;
    } else {
        for (i = 0; i < EFFECT_PARTICLE_CHANNEL_COUNT; i++) {
            effect_step_emitter_timeline(record, &g_effect_timeline->particle_channels[0][i],
                &timeline->state.parent.keyframe.tracks.particle[0][i],
                &timeline->state.parent.remaining.tracks.particle[0][i],
                &timeline->state.parent.step.tracks.particle[0][i]);
        }
        effect_advance_target_palette_track(&g_effect_timeline->color_tracks[0].surroundings,
            &timeline->state.parent.keyframe.tracks.surroundings[0],
            &timeline->state.parent.remaining.tracks.surroundings[0]);
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            effect_advance_caster_palette_track(&g_effect_timeline->color_tracks[0].caster,
                &timeline->state.parent.keyframe.tracks.caster[0], &timeline->state.parent.remaining.tracks.caster[0],
                g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id);
        }
        effect_advance_coord_target_palette_track(&g_effect_timeline->color_tracks[0].target,
            &timeline->state.parent.keyframe.tracks.target[0], &timeline->state.parent.remaining.tracks.target[0]);
        effect_advance_map_background_track(&g_effect_timeline->color_tracks[0].screen,
            &timeline->state.parent.keyframe.tracks.screen[0], &timeline->state.parent.remaining.tracks.screen[0]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[0][0],
            &timeline->state.parent.keyframe.tracks.sound[0][0], &timeline->state.parent.remaining.tracks.sound[0][0],
            &timeline->state.parent.step.tracks._unknown_24[0][0][1]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[0][1],
            &timeline->state.parent.keyframe.tracks.sound[1][0], &timeline->state.parent.remaining.tracks.sound[1][0],
            &timeline->state.parent.step.tracks._unknown_24[1][0][1]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[0][2],
            &timeline->state.parent.keyframe.tracks.sound[2][0], &timeline->state.parent.remaining.tracks.sound[2][0],
            &timeline->state.parent.step.tracks._unknown_24[2][0][1]);
        if (g_effect_flags_section->flags & EFFECT_FLAG_TIMING_CURVE_PHASE1) {
            battle_state_set_time_scale(effect_get_timing_curve_value(record));
        }
    }
    i = g_effect_timeline->phase1_duration + (g_effect_context.target_count - 1) * g_effect_timeline->spawn_delay
        + g_effect_timeline->child.duration;
    if (timeline->frame >= i) {
        for (i = 0; i < EFFECT_PARTICLE_CHANNEL_COUNT; i++) {
            effect_step_emitter_timeline(record, &g_effect_timeline->particle_channels[1][i],
                &timeline->state.parent.keyframe.tracks.particle[1][i],
                &timeline->state.parent.remaining.tracks.particle[1][i],
                &timeline->state.parent.step.tracks.particle[1][i]);
        }
        effect_advance_target_palette_track(&g_effect_timeline->color_tracks[1].surroundings,
            &timeline->state.parent.keyframe.tracks.surroundings[1],
            &timeline->state.parent.remaining.tracks.surroundings[1]);
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            effect_advance_caster_palette_track(&g_effect_timeline->color_tracks[1].caster,
                &timeline->state.parent.keyframe.tracks.caster[1], &timeline->state.parent.remaining.tracks.caster[1],
                g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id);
        }
        effect_advance_coord_target_palette_track(&g_effect_timeline->color_tracks[1].target,
            &timeline->state.parent.keyframe.tracks.target[1], &timeline->state.parent.remaining.tracks.target[1]);
        effect_advance_map_background_track(&g_effect_timeline->color_tracks[1].screen,
            &timeline->state.parent.keyframe.tracks.screen[1], &timeline->state.parent.remaining.tracks.screen[1]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[1][0],
            &timeline->state.parent.keyframe.tracks.sound[0][1], &timeline->state.parent.remaining.tracks.sound[0][1],
            &timeline->state.parent.step.tracks._unknown_24[0][1][1]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[1][1],
            &timeline->state.parent.keyframe.tracks.sound[1][1], &timeline->state.parent.remaining.tracks.sound[1][1],
            &timeline->state.parent.step.tracks._unknown_24[1][1][1]);
        effect_advance_parent_sound_track(&g_effect_timeline->sound_tracks[1][2],
            &timeline->state.parent.keyframe.tracks.sound[2][1], &timeline->state.parent.remaining.tracks.sound[2][1],
            &timeline->state.parent.step.tracks._unknown_24[2][1][1]);
    }
    camera_advance_effect_tracks(timeline->frame);
    timeline->frame++;
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

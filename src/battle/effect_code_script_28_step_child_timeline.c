#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/suzuki.h"
#include "psx/types.h"

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

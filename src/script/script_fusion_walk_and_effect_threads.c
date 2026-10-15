/* Camera fusion thread and track step, walk/jump/mirror/palette instructions, effect playback and block/add-unit
 * threads. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/stddef.h"
#include "psx/types.h"

void camera_fusion_thread(void) {
    camera_track_t* tracks;
    camera_track_t* track;
    camera_key_t* key;
    u8* script;
    u8* cursor;
    s32 values[EVENT_CAMERA_COMPONENT_COUNT];
    s32 key_count;
    s32 key_index;
    s32 i;
    s16 value;
    s32 component;
    s32 opcode;

    thread_set_current_task_id(THREAD_TASK_CAMERA);
    tracks = menu_alloc_memory(sizeof(camera_track_t) * EVENT_CAMERA_COMPONENT_COUNT);
    script = (u8*)thread_get_current_parameter_1_variant();
    menu_clear_render_buffer(tracks, sizeof(camera_track_t) * EVENT_CAMERA_COMPONENT_COUNT);

    /* One key per CAMERA instruction up to the end marker, plus the starting key. */
    cursor = script;
    key_count = 0;
    for (;;) {
        opcode = *cursor;
        if (opcode == EVENT_OPCODE_CAMERA) {
            key_count++;
        }
        if (opcode == EVENT_OPCODE_CAMERA_FUSION_END) {
            break;
        }
        cursor += g_script_event_instruction_sizes[opcode] + 1;
    }
    key_count++;

    cursor = script;
    camera_store_yaw_band(g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW]);

    /* Key 0 is the camera as it stands. Each later key is the next CAMERA
     * instruction, after the Focus, RelativeCamera and FocusSpeed instructions
     * in front of it have run. */
    for (key_index = 0; key_index < key_count; key_index++) {
        if (key_index == 0) {
            for (i = 0; i < EVENT_CAMERA_COMPONENT_COUNT; i++) {
                track = &tracks[i];
                track->keys[key_index].value = g_script_variables[g_camera_script_variable_indices[i]];
                values[i] = track->keys[key_index].value;
                if (i < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                    track->keys[key_index].value = track->keys[key_index].value / 1024;
                }
                track->keys[key_index].time = 0;
                track->key_count = key_count;
            }
        } else {
            for (;;) {
                if (*cursor == EVENT_OPCODE_FOCUS) {
                    script_focus((event_focus_parameters_t*)(cursor + 1));
                } else if (*cursor == EVENT_OPCODE_RELATIVE_CAMERA) {
                    script_apply_relative_camera((event_camera_values_t*)(cursor + 1), values);
                } else if (*cursor == EVENT_OPCODE_FOCUS_SPEED) {
                    script_focus_speed(cursor + 1, values, values + EVENT_CAMERA_COMPONENT_TILT);
                } else if (*cursor == EVENT_OPCODE_CAMERA) {
                    for (i = 0; i < EVENT_CAMERA_COMPONENT_COUNT; i++) {
                        /* Indexed field access replaces retail's two additions with a shift. */
                        value
                            = script_read_s16(cursor + i + i + offsetof(event_camera_instruction_t, parameters.values));
                        if (value == CAMERA_FUSION_HOLD_SENTINEL) {
                            if (i < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                                value = values[i] / 1024;
                            } else {
                                value = values[i];
                            }
                        }
                        key = &tracks[i].keys[key_index];
                        key->value = value;
                        key->time = script_read_s16(((event_camera_instruction_t*)cursor)->parameters.duration_le);
                        key->time += tracks[i].keys[key_index - 1].time;
                        component = key->value;
                        values[i] = component;
                        if (i < EVENT_CAMERA_POSITION_COMPONENT_COUNT) {
                            values[i] = component << 10;
                        }
                        if (i == EVENT_CAMERA_COMPONENT_YAW) {
                            camera_store_yaw_band(key->value);
                        }
                    }
                    break;
                }
                cursor += g_script_event_instruction_sizes[*cursor] + 1;
            }
            cursor += g_script_event_instruction_sizes[*cursor] + 1;
        }
    }

    /* Play the tracks; the three position tracks stand still while the position is locked. */
    for (;;) {
        track = tracks;
        for (key_index = 0; key_index < EVENT_CAMERA_POSITION_COMPONENT_COUNT; key_index++, track++) {
            if (g_camera_position_locked == 0) {
                g_script_variables[g_camera_script_variable_indices[key_index]] = camera_step_track(track, key_index);
            }
        }
        for (key_index = EVENT_CAMERA_COMPONENT_TILT; key_index < EVENT_CAMERA_COMPONENT_COUNT; key_index++, track++) {
            g_script_variables[g_camera_script_variable_indices[key_index]] = camera_step_track(track, key_index);
        }
        if (tracks[EVENT_CAMERA_COMPONENT_TILT].finished == 1) {
            break;
        }
        camera_clear_zoom_variable_low_bit();
        thread_yield();
    }

    menu_free_memory(tracks);
    thread_exit_current();
}

/* Advances one camera-track component by a frame and returns its new value.
 * Called once for each component by the scene's camera-fusion thread.
 *
 * Each call samples the two tangent lines of the active key segment at this
 * frame and the next, intersects them (fixed_cross_product_q12 is a Q12 cross product
 * that sets g_camera_cross_product_overflow on overflow) and moves the value along the resulting
 * control point. The locals span, frame and lead are reused across the
 * stages, as the target's register map requires. */
s32 camera_step_track(camera_track_t* track, s32 component) {
    s32 time[4];  /* line 0: time[0]->time[1]; line 1: time[2]->time[3] */
    s32 value[4]; /* matching values for the two sampled lines */
    s32 value_delta0;
    s32 flag;
    s32 i;
    s32 frame;
    s32 span;
    s32 result;
    s32 segment;
    s32 start_value;
    s32 t0;
    s32 t1;
    s32 t2;
    s32 lead;
    s32 time_delta0;
    s32 time_delta1;
    s32 value_delta1;
    s32 divisor;
    s32 time0;
    s32 time1;
    s32 time2;
    s32 time3;
    s32 value0;
    s32 value1;
    s32 value2;
    s32 value3;

    /* Entering a segment: its length in frames and the value it starts from. */
    if (track->segment_frame == 0) {
        segment = track->segment;
        t2 = track->keys[segment + 2].time;
        t1 = track->keys[segment + 1].time;
        t0 = track->keys[segment].time;
        span = t2 - t1;
        lead = t1 - t0;
        frame = t2 - t0; /* whole two-key span */
        if (track->key_count >= 4) {
            if (segment == 0) {
                track->segment_duration = span / 2 + lead;
            } else if (segment == track->key_count - 3) {
                track->segment_duration = span + lead / 2;
            } else {
                track->segment_duration = frame / 2;
            }
        } else {
            track->segment_duration = frame;
        }
        start_value = track->keys[track->segment].value;
        track->prev_value = start_value;
        if (track->key_count >= 4 && track->segment != 0) {
            track->prev_value
                = (track->keys[track->segment + 1].value - track->keys[track->segment].value) / 2 + start_value;
        }
        track->prev_value *= ONE;
        track->prev_time = track->frame * ONE;
        track->start_value = track->prev_value;
    }

    /* Three equal keys hold their value. */
    if (track->keys[track->segment].value == track->keys[track->segment + 1].value
        && track->keys[track->segment].value == track->keys[track->segment + 2].value) {
        result = track->keys[track->segment].value * ONE;
    } else {
        for (i = 0; i < 2; i++) {
            frame = i + track->segment_frame;
            if (track->segment == 0) {
                time[i * 2] = script_interpolate_range_fixed12(track->keys[track->segment].time,
                    track->keys[track->segment + 1].time, frame, track->segment_duration);
                value[i * 2] = script_interpolate_range_fixed12(track->keys[track->segment].value,
                    track->keys[track->segment + 1].value, frame, track->segment_duration);
            } else {
                time[i * 2] = script_interpolate_range_fixed12(track->keys[track->segment].time,
                                  track->keys[track->segment + 1].time, frame, track->segment_duration * 2)
                    - track->keys[track->segment].time
                    + (track->keys[track->segment + 1].time - track->keys[track->segment].time) / 2;
                value[i * 2] = script_interpolate_range_fixed12(track->keys[track->segment].value,
                                   track->keys[track->segment + 1].value, frame, track->segment_duration * 2)
                    - track->keys[track->segment].value
                    + (track->keys[track->segment + 1].value - track->keys[track->segment].value) / 2;
            }
            span = track->segment_duration;
            if (track->segment != track->key_count - 3) {
                span *= 2;
            }
            time[i * 2 + 1] = script_interpolate_range_fixed12(
                track->keys[track->segment + 1].time, track->keys[track->segment + 2].time, frame, span);
            value[i * 2 + 1] = script_interpolate_range_fixed12(
                track->keys[track->segment + 1].value, track->keys[track->segment + 2].value, frame, span);
        }
        flag = 0;
        g_camera_cross_product_overflow = 0;
        value1 = value[1];
        value0 = value[0];
        time3 = time[3];
        time2 = time[2];
        time0 = time[0];
        value3 = value[3];
        value2 = value[2];
        time1 = time[1];
        value_delta0 = value1 - value0;
        time_delta1 = time3 - time2;
        value_delta1 = value3 - value2;
        time_delta0 = time1 - time0;
        divisor = fixed_cross_product_q12(value_delta0, time_delta1, time_delta0, value_delta1);
        result = fixed_cross_product_q12(time_delta1, value2 - value0, value_delta1, time2 - time0);
        if (divisor != 0) {
            span = mul_div_s64(time_delta0, result, divisor) + time0;   /* crossing time */
            lead = mul_div_s64(value_delta0, result, divisor) + value0; /* crossing value */
            divisor = span - track->prev_time;
            if (divisor != 0) {
                result = mul_div_s64(lead - track->prev_value, (track->frame * ONE) - track->prev_time, divisor)
                    + track->prev_value;
                track->prev_time = span;
                track->prev_value = lead;
            } else {
                flag = 1;
            }
        } else {
            flag = 1;
        }
        if (g_camera_cross_product_overflow != 0 || flag != 0) {
            result = track->prev_value;
        }
    }

    track->frame++;
    if (++track->segment_frame >= track->segment_duration) {
        track->segment_frame = 0;
        if (track->key_count - 3 < ++track->segment) {
            track->finished = 1;
        }
    }
    /* Angles and zoom round Q12 to the nearest integer; positions return in
     * the 1/1024 units of their script variables. */
    if (component >= EVENT_CAMERA_COMPONENT_TILT) {
        if (result > 0) {
            if ((result & 0xFFF) > 0x800) {
                result += ONE;
            }
        } else if ((result & 0xFFF) < 0x800) {
            result -= ONE;
        }
        return result / ONE;
    }
    return result / 4;
}

/* Linear interpolation from start to end at fraction / value, returned in 20.12
 * fixed point: start * ONE + (end - start) * fraction * ONE / value. */
s32 script_interpolate_range_fixed12(s32 start, s32 end, s32 fraction, s32 value) {
    return mul_div_s64(end - start, fraction * ONE, value) + (start * ONE);
}

void script_print_debug_message(void) {
    FntPrint(g_script_pause_message);
    thread_yield();
    g_script_debug_paused = 1;
}

/* Start movement to the encoded tile with the signed speed and terrain-cost flag. */
void script_walkto_event_instruction(const event_walk_to_parameters_t* parameters) {
    s32 id;
    map_tile_coordinates_t coordinates;

    id = script_read_s16(parameters->unit_id_le);
    id = script_resolve_runtime_id(id);
    if (id != EVENT_RUNTIME_ID_NONE) {
        coordinates.x = parameters->x;
        coordinates.y = parameters->y;
        coordinates.layer = parameters->elevation;
        move_start_unit_walk_to(id, &coordinates,
            MOVE_PATHFIND_IGNORE_TERRAIN_COST - parameters->terrain_cost_flag * MOVE_PATHFIND_IGNORE_TERRAIN_COST, 3,
            script_read_s16(parameters->speed_le));
    }
}

/* Mirror the unit sprite for mode 1 and restore it for every other mode. */
void script_mirrorsprite_event_instruction(const event_mirror_sprite_parameters_t* parameters) {
    s32 mode;
    s32 runtime_id;

    runtime_id = script_read_s16(parameters->unit_id_le);
    mode = parameters->mode;
    runtime_id = script_resolve_runtime_id(runtime_id);
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        if (mode == 1) {
            battle_unit_set_horizontal_flip_flag(runtime_id);
        } else {
            battle_unit_clear_horizontal_flip_flag(runtime_id);
        }
    }
}

/* ResetPalette: resolve the unit id operand and reapply the weather, time-of-day
 * palette modulation without tile effects, unless the unit is missing. */
void script_resetpalette_event_instruction(const event_unit_parameters_t* parameters) {
    s32 runtime_id = script_resolve_runtime_id(script_read_s16(parameters->unit_id_le));
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        battle_gfx_apply_weather_time_tile_palette_mod_by_runtime_id(runtime_id);
    }
}

/* Jump: cancel the unit's accumulated event offset, then build its one-step path
 * of length_plus_one - 1 tiles in the encoded direction through
 * battle_unit_prepare_rider_dismount. The unit's Jump stat is 6 while that runs
 * and is restored afterwards. A unit id that does not resolve does nothing. */
void script_jump_event_instruction(const event_jump_parameters_t* parameters) {
    SVECTOR vec;
    s32 runtime_id;
    SVECTOR* coords;
    unit_stats_t* stats;
    u8 saved;

    runtime_id = script_resolve_runtime_id(script_read_s16(parameters->unit_id_le));
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        coords = battle_unit_get_event_offset_by_runtime_id(runtime_id);
        setVector(&vec, -coords->vx, -coords->vy, -coords->vz);
        battle_unit_add_event_offset_by_runtime_id(runtime_id, &vec);
        stats = battle_unit_get_stats_from_battle_id(battle_unit_get_battle_id_by_runtime_id(runtime_id));
        saved = stats->jump;
        stats->jump = 6;
        battle_unit_prepare_rider_dismount(runtime_id, parameters->direction, parameters->length_plus_one - 1);
        stats->jump = saved;
    }
}

/* Yield at least once, then wait until the resolved unit stops moving. */
void script_waitwalk_event_instruction(s32 unit_id) {
    s32 runtime_id;

    runtime_id = script_resolve_runtime_id(unit_id);
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        do {
            thread_yield();
        } while (move_is_unit_moving_by_runtime_id(runtime_id) != 0);
    }
}

/* Enter ATTACK.OUT when script variable 0x1fc is clear: mark the transition,
 * load companion executable 0xd, then hand off to its scenario-music,
 * formation-sprite, and portrait entry points. */
void script_load_attack_graphics_event_instruction(void) {
    if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) == 0) {
        g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_PRELOADED;
        menu_request_open_companion_executable(COMPANION_OVERLAY_ATTACK);
        thread_yield();
        attack_load_scenario_conditionals();
        attack_gfx_build_formation_sprites();
        attack_gfx_load_portraits();
    }
}

/* Plays the current effect through the inner-subroutine caller, then yields
 * to other threads until the animation continue check reports done. */
void script_play_effect_and_wait(void) {
    g_thread_call_target = effect_call_play;
    thread_call_on_main_stack();

    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))battle_state_get_animation_continue_check;
    } while (thread_call_on_main_stack());
}

/* Thread body of EffectEnd: play the effect that Effect and EffectStart prepared
 * and exit when it has finished. */
void script_play_effect_thread(void) {
    thread_set_current_task_id(THREAD_TASK_PLAY_EFFECT);
    script_play_effect_and_wait();
    thread_exit_current();
}

/* Run the unit instructions inside an event block, then exit its thread. */
void script_block_start_thread(void) {
    /* Preserve the target's unused 40-byte stack area. */
    s32 unused[10];
    u8* script;
    u8* parameters;
    s16 first_halfword;
    s16 runtime_id;
    s32 thread_id;

    g_thread_contexts[g_thread_current_id].task_id = THREAD_TASK_EVENT_BLOCK;
    script = (u8*)thread_get_current_parameter_1_variant();
    parameters = script + 1;
    while (script[0] != EVENT_OPCODE_BLOCK_END) {
        first_halfword = script_read_s16(parameters);
        if (script[0] == EVENT_OPCODE_CALL_FUNCTION) {
            if (script[1] == EVENT_CALL_FUNCTION_MUTE_TEXT_AUDIO_CUE) {
                script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 1);
            } else if (script[1] == EVENT_CALL_FUNCTION_UNMUTE_TEXT_AUDIO_CUE) {
                script_set_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE, 0);
            }
        } else if (script[0] == EVENT_OPCODE_SOUND_EFFECT) {
            g_sound_pending_sfx_id = first_halfword;
        } else if (script[0] == EVENT_OPCODE_WARP_UNIT) {
            script_warp_unit((const event_warp_unit_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_WAIT_VALUE) {
            script_wait_value_event_instruction((const event_wait_value_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_DEBUG_PAUSE) {
            script_print_debug_message();
        } else if (script[0] == EVENT_OPCODE_WALK_TO) {
            g_thread_call_target = (void (*)(void))script_walkto_event_instruction;
            thread_call_on_main_stack(parameters);
        } else if (script[0] == EVENT_OPCODE_WALK_TO_ANIM) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                g_unit_pending_animation_ids[runtime_id] = script_read_s16(script + 3);
            }
            g_unit_was_moving_latch[runtime_id] = 0;
        } else if (script[0] == EVENT_OPCODE_JUMP) {
            script_jump_event_instruction((const event_jump_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_WAIT_WALK) {
            script_waitwalk_event_instruction(first_halfword);
        } else if (script[0] == EVENT_OPCODE_UNIT_ANIM) {
            script_unit_anim((const event_unit_anim_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_MIRROR_SPRITE) {
            script_mirrorsprite_event_instruction((const event_mirror_sprite_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_RESET_PALETTE) {
            script_resetpalette_event_instruction((const event_unit_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_SET_FIRST_UNIT) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_unit_move_to_head(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_ERASE) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_reset_unit_graphic_trigger(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_DRAW) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_unit_draw_and_activate_by_runtime_id(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_WAIT_UNIT_READY) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                script_wait_for_unit_ready(runtime_id);
            }
        } else if (script[0] == EVENT_OPCODE_WAIT) {
            thread_wait_frames(script_read_s16(script + 1));
        } else if (script[0] == EVENT_OPCODE_ROTATE_UNIT) {
            script_rotate_unit((const event_rotate_unit_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_FACE_UNIT) {
            script_face_unit((const event_face_unit_parameters_t*)(script + 1), 1);
        } else if (script[0] == EVENT_OPCODE_SPRITE_MOVE || script[0] == EVENT_OPCODE_SPRITE_MOVE_BETA) {
            thread_id = thread_resolve_id_after_current(THREAD_SLOT_FIRST_IDLE);
            if (script[0] == EVENT_OPCODE_SPRITE_MOVE) {
                thread_start(thread_id, script_sprite_move);
            } else {
                thread_start(thread_id, script_sprite_move_beta);
            }
            g_thread_contexts[thread_id].function_parameter_1 = (s32)(script + 1);
            g_thread_contexts[thread_id].task_id = THREAD_TASK_SPRITE_MOVE;
            g_thread_contexts[thread_id].task_words[0] = script_resolve_runtime_id(first_halfword);
        } else if (script[0] == EVENT_OPCODE_WAIT_SPRITE_MOVE) {
            script_waitspritemove_event_instruction(first_halfword);
        } else if (script[0] == EVENT_OPCODE_WAIT_ROTATE_UNIT) {
            script_waitrotateunit_and_waitrotateall_event_instruction(script[1]);
        } else if (script[0] == EVENT_OPCODE_WAIT_ROTATE_ALL) {
            script_waitrotateunit_and_waitrotateall_event_instruction(-1);
        } else if (script[0] == EVENT_OPCODE_UNIT_ANIM_ROTATE) {
            script_unit_animation_rotate_event_instruction((const event_unit_anim_rotate_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_COLOR_UNIT) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                battle_gfx_apply_unit_palette_modulation_to_units(
                    script[3], script[7], runtime_id, (s8)script[4], (s8)script[5], (s8)script[6]);
            }
        }
        script += 1 + g_script_event_instruction_sizes[script[0]];
        parameters = script + 1;
    }
    thread_exit_current();
}

/*
 * Event thread started by the AddUnitStart instruction: walks the following
 * script instructions through AddUnitEnd, loading each AddUnit's graphics and
 * processing LoadAttackGraphics before waiting for pending loads. WORLD binds
 * the thread parameter accessor to its own copy.
 */
void script_add_unit_start_thread(void) {
    s32 index;
    s32 slot;
    const u8* script;
    unit_stats_t* unit;
    s32 runtime_id;
    s16 unit_id;
    u8 opcode;
    u8 flag;

    g_thread_contexts[g_thread_current_id].task_id = THREAD_TASK_ADD_UNIT;
    slot = 0;
    script = (const u8*)thread_get_current_parameter_1_variant();
    while (1) {
        unit_id = script_read_s16(script + 1);
        opcode = script[0];
        flag = script[3];
        if (opcode == EVENT_OPCODE_ADD_UNIT_END) {
            break;
        }
        if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            script_load_attack_graphics_event_instruction();
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            unit = battle_unit_find_stats_by_entd_unit_id(unit_id, &index);
            if (index == -2 || index >= 0) {
                for (slot = 0; slot < EVENT_UNIT_SLOT_COUNT; slot++) {
                    if (battle_unit_get_stats_from_battle_id(slot) == unit) {
                        break;
                    }
                }
                runtime_id = battle_unit_get_runtime_id_by_battle_id(slot);
                if (runtime_id == -1) {
                    unit_save_staged_status_data(slot);
                    battle_gfx_load_unit_graphics_by_battle_id(slot, flag);
                    g_unit_graphics_load_pending = 1;
                } else {
                    if (flag == 0) {
                        battle_unit_draw_and_activate_by_runtime_id(runtime_id);
                    }
                    if (unit_save_staged_status_data(slot) != 0) {
                        g_thread_call_target = (void (*)(void))battle_unit_update_graphics_by_runtime_id;
                        thread_call_on_main_stack(runtime_id);
                    }
                }
            }
            while (1) {
                thread_yield();
                if (g_unit_graphics_load_pending == 0) {
                    break;
                }
                if (g_main_debug_display_enabled != 0) {
                    FntPrint(g_text_reading_character_message, slot);
                }
            }
        }
        slot++;
        script += g_script_event_instruction_sizes[script[0]] + 1;
    }
    thread_exit_current();
}

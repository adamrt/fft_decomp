/* Attack resources and threads, the unit view and mini menu threads, event speed, debug printing, the walk and effect
 * script instructions, subsystem init and reset, selection records, skillset entries and the skill menu. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "fft/camera.h"
#include "fft/data.h"
#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_item.h"
#include "fft/map.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/menu_panel.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

static s32 camera_step_track(camera_track_t* track, s32 component);
static void map_store_selected_tile_data(void);
static void menu_init_subsystems(void);
static void menu_run_unit_view_supervisor_thread(void);
static s32 script_interpolate_range_fixed12(s32 start, s32 end, s32 fraction, s32 value);
static void unit_reset_animation_states(void);

/* Load ATTACK.OUT (unless preloaded) with its scenario conditions and graphics, reset menu
 * and unit state and, for a tutorial, fix the game options. Then start the deployment
 * screen and return 0, or start the map-load thread and return 1 when deployment is skipped. */
s32 menu_init_attack_resources_and_threads(void) {
    s32 result;
    s32 return_value;
    game_options_fields_t* options;
    g_menu_hide_numeric_values = 0;
    menu_init_subsystems();
    unit_reset_animation_states();
    menu_reset_unit_records();
    if (g_script_attack_entry_mode == SCRIPT_ATTACK_ENTRY_PRELOADED) {
        g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_PRELOAD_USED;
    } else {
        g_thread_call_target = main_file_load_checked_to_address;
        thread_call_on_main_stack(DISC_SECTOR_EVENT_ATTACK_OUT, 0x20000, g_main_heap_companion_overlay_load_address);
    }
    result = attack_load_scenario_conditionals();
    if (g_script_attack_entry_mode != SCRIPT_ATTACK_ENTRY_PRELOAD_USED) {
        attack_gfx_build_formation_sprites();
        attack_gfx_load_portraits();
    }
    g_menu_panel_fade_mode = MENU_PANEL_FADE_NONE;
    g_menu_panel_fade_intensity = 0;
    if (script_is_tutorial_event_slot() != 0) {
        g_gfx_screen_fade = 0xff;
        g_menu_saved_game_options = g_main_game_options;
        options = &g_main_game_options.fields;
        options->cursor_movement = GAME_CURSOR_MOVEMENT_TYPE_A;
        options->multi_height_cursor_speed = GAME_MULTI_HEIGHT_CURSOR_SPEED_REGULAR;
        options->navigation_messages = GAME_OPTION_ON;
        options->effect_messages = GAME_OPTION_OFF;
        options->target_flashing = GAME_OPTION_ON;
        options->show_unequippable_items = GAME_OPTION_ON;
    } else {
        g_gfx_screen_fade = 0;
    }
    attack_deploy_find_fieldable_units();
    if (result != 0) {
        thread_start(THREAD_BORROWED_SLOT_DEPLOYMENT, map_data_load_complete_thread);
        return_value = 1;
    } else {
        g_sound_music_track_1_id = 0;
        g_sound_music_track_2_id = 0;
        sound_play_music_tracks(MAIN_MUSIC_DEPLOYMENT, MAIN_MUSIC_NONE);
        attack_text_build_deployment_strings();
        attack_map_load_title_graphic();
        thread_start(THREAD_BORROWED_SLOT_DEPLOYMENT, attack_deploy_run_screen);
        return_value = 0;
    }
    return return_value;
}

/* Start the mini menu when its slots are idle and menus are active. */
void menu_start_mini_menu_display_thread(void) {
    if (thread_is_running_asm(THREAD_SLOT_MENU) != 0) {
        return;
    }
    if (thread_is_running_asm(THREAD_SLOT_MENU_CHILD) != 0) {
        return;
    }
    if (g_menu_overlay_state != 0) {
        return;
    }
    g_menu_current_id = BATTLE_MENU_ID_MINI_MENU;
    g_menu_thread_menu_data = g_menu_idle_action_entries;
    thread_start(THREAD_SLOT_MENU, menu_run_triangle_selection);
    thread_set_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_MINI_MENU], 0, 0);
    g_thread_contexts[THREAD_SLOT_MENU].task_id = THREAD_TASK_MINI_MENU;
}

#ifdef FFT_BATTLE
/* Report whether the menu layer is busy.
 *
 * Returns g_menu_transition_state when it is nonzero (4 during the unit
 * list fade-out), 2 while the menu-building thread runs or while menu input is
 * blocked, 4 when one of threads 4-9 is running, and 0 when all are idle. The `i
 * != 3` test inside the scan of threads 4-9 always passes. */
s32 menu_get_thread_activity_variant(void) {
    s32 i;
    s32 result;

    if (g_menu_transition_state != 0) {
        return g_menu_transition_state;
    }
    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        return 2;
    }
    i = 4;
    do {
        if (i != 3) {
            if (thread_is_running_asm(i) != 0) {
                break;
            }
        }
        i += 1;
    } while (i < 0xA);
    if (g_menu_input_mode != MENU_INPUT_BLOCKED) {
        result = i != 0xA;
        return result * 4;
    }
    return 2;
}
#endif

#ifdef FFT_WORLD
/* Menu activity summary: 2 while the menu-building thread runs, else 4 when
 * any of thread slots 4..8 runs, else 0. */
s32 menu_get_thread_activity_variant(void) {
    s32 i;

    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        return 2;
    }
    for (i = 4; i < 9; i++) {
        if (thread_is_running_asm(i) != 0) {
            break;
        }
    }
    return (i != 9) << 2;
}
#endif

/* Raise the flag that releases menu_at_list_select_unit_thread once a unit picked
 * from the AT list has been viewed. */
void menu_finish_at_list_selection(void) {
    g_menu_at_list_selection_finished = 1;
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_finish_at_list_selection and menu_noop_6. Nothing on the disc calls it
 * or points at it; the name is a neutral placeholder until a reference turns
 * up. */
void menu_noop_5(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between menu_noop_5 and
 * menu_start_unit_view_thread. Nothing on the disc calls it or points at it;
 * the name is a neutral placeholder until a reference turns up. */
void menu_noop_6(void) {
}

/* Requests a unit view mode from the unit view supervisor thread, starting the
 * thread when it is not running.
 *
 * The unit name list is rebuilt and the panel fade mode taken from
 * g_menu_panel_fade_modes[mode]. unit_a becomes the active-turn unit of
 * the banner and unit_b the previewed target of the status billboard; 0xff leaves
 * either unchanged. Ignored while g_menu_overlay_state is 1. */
void menu_start_unit_view_thread(s32 mode, s32 unit_a, s32 unit_b) {
    if (g_menu_overlay_state == 1) {
        return;
    }
    text_build_unit_name_list();
    g_menu_panel_fade_mode = g_menu_panel_fade_modes[mode];
    if (unit_a != 0xff) {
        g_menu_selected_unit.battle_id = unit_a;
        g_menu_selected_unit_status.unit_index = unit_a;
    }
    if (unit_b != 0xff) {
        g_menu_comparison_unit_data.battle_id = unit_b;
        g_menu_comparison_unit_status.unit_index = unit_b;
    }
    if (thread_is_running_asm(THREAD_SLOT_UNIT_VIEW) == 0) {
        thread_start(THREAD_SLOT_UNIT_VIEW, menu_run_unit_view_supervisor_thread);
    }
    thread_set_parameters(THREAD_SLOT_UNIT_VIEW, mode, 0, 0);
}

enum {
    MENU_UNIT_VIEW_SLOT_IDLE = 0,
    MENU_UNIT_VIEW_SLOT_RESTART = 1,
    MENU_UNIT_VIEW_SLOT_STARTED = 2,
};

/* Starts the requested unit-view panel threads, stops stale panels, and restarts
 * them after they exit. A new parameter replaces the pending view request. */
static void menu_run_unit_view_supervisor_thread(void) {
    s32 states[16];
    menu_unit_view_thread_set_t* set;
    s32 mode;
    s32 i;
    s32 task;
    thread_t* thread;

    for (;;) {
        mode = thread_get_current_parameter_1();
        set = &g_menu_unit_view_thread_sets[mode];
        g_menu_unit_view_mode = mode;
        g_thread_contexts[g_thread_current_id].function_parameter_1 = 0xff;
        for (i = 15; i >= 0; i--) {
            states[i] = MENU_UNIT_VIEW_SLOT_IDLE;
        }
        for (i = 0; i < 4; i++) {
            thread = &g_thread_contexts[set->thread_ids[i]];
            if (thread->is_running != 0) {
                task = thread->task_id;
                if (task == set->task_ids[i]
                    && (task != THREAD_TASK_HOVERED_UNIT_STATS
                        || g_menu_unit_view_cached_battle_id == g_menu_selected_unit.battle_id)
                    && (task != THREAD_TASK_RIGHT_UNIT_STATS
                        || g_menu_unit_view_cached_comparison_id == g_menu_comparison_unit_data.battle_id)
                    && (task != THREAD_TASK_UNIT_SUMMARY_PANEL
                        || g_menu_unit_view_cached_battle_id == g_menu_selected_unit.battle_id)) {
                    continue;
                }
                states[set->thread_ids[i]] = MENU_UNIT_VIEW_SLOT_RESTART;
                thread_set_parameters(set->thread_ids[i], 0, 0, 1);
            } else if (set->entries[i] != (void*)-1) {
                thread_start(set->thread_ids[i], set->entries[i]);
                thread_set_parameters(set->thread_ids[i], 0, 0, 0);
                thread->task_id = set->task_ids[i];
                states[set->thread_ids[i]] = MENU_UNIT_VIEW_SLOT_STARTED;
            }
        }
        g_menu_unit_view_cached_battle_id = g_menu_selected_unit.battle_id;
        g_menu_unit_view_cached_comparison_id = g_menu_comparison_unit_data.battle_id;
        for (;;) {
            for (i = 0; i < 4; i++) {
                if (states[set->thread_ids[i]] == MENU_UNIT_VIEW_SLOT_RESTART
                    && g_thread_contexts[set->thread_ids[i]].is_running != 0) {
                    break;
                }
            }
            if (i == 4) {
                break;
            }
            thread_wait_frames(1);
        }
        for (i = 0; i < 4; i++) {
            if (states[set->thread_ids[i]] == MENU_UNIT_VIEW_SLOT_RESTART && set->entries[i] != (void*)-1) {
                thread_start(set->thread_ids[i], set->entries[i]);
                thread_set_parameters(set->thread_ids[i], 0, 0, 0);
                g_thread_contexts[set->thread_ids[i]].task_id = set->task_ids[i];
            }
        }
        if (thread_get_current_parameter_1() == 0xff) {
            thread_exit_current();
        }
    }
}

/* Set the event speed, which also divides the input repeat delays; BATTLE
 * sets the animation speed with it. */
void script_set_event_speed(s32 speed) {
    g_script_event_speed = speed;
#ifdef FFT_BATTLE
    battle_state_set_animation_speed(speed);
#endif
}

/* Yield until the four unit view panel threads (hovered and right unit stats, unit summary, tile
 * info) have stopped. */
void thread_wait_for_unit_view_panels(void) {
    thread_wait_until_inactive(THREAD_SLOT_HOVERED_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_RIGHT_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_UNIT_SUMMARY_PANEL);
    thread_wait_until_inactive(THREAD_SLOT_TILE_INFO_PANEL);
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * thread_wait_for_unit_view_panels and script_noop_5. Nothing on the disc
 * calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_4(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_noop_4 and
 * script_frame_after_threads_stub. Nothing on the disc calls it or points at
 * it; the name is a neutral placeholder until a reference turns up. */
void script_noop_5(void) {
}

/* Empty. The event frame runner calls it each frame once the threads have run. */
void script_frame_after_threads_stub(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_frame_after_threads_stub
 * and script_noop_7. Nothing on the disc calls it or points at it; the name is a
 * neutral placeholder until a reference turns up. */
void script_noop_6(void) {
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_noop_6 and
 * script_assign_bitset_flag. Nothing on the disc calls it or points at it; the
 * name is a neutral placeholder until a reference turns up. */
void script_noop_7(void) {
}

/* Set or clear one flag in a 32-bit-word bitmap. */
static void script_assign_bitset_flag(u32* bits, s32 index, s32 set) {
    s32 word = index / 32;
    u32 mask = 1U << (index & 0x1F);

    if (set != 0) {
        bits[word] |= mask;
    } else {
        bits[word] &= ~mask;
    }
}

/* Store five day bits and four month bits in the indexed nine-bit date record. */
void script_opcode_unlock_date(u32* bits, s32 record, s32 month, s32 day) {
    s32 base = record * 9;
    s32 i;
    u8 mask = 1;

    for (i = 0; i < 5; i++) {
        script_assign_bitset_flag(bits, base + i, day & mask);
        mask <<= 1;
    }
    mask = 1;
    for (i = 0; i < 4; i++) {
        s32 bit = i + 5;
        script_assign_bitset_flag(bits, base + bit, month & mask);
        mask <<= 1;
    }
}

/* CameraFusion thread: take the CAMERA instructions up to the end marker as keyframes (key 0 is
 * the current camera; Focus, RelativeCamera and FocusSpeed between them adjust the values), then
 * move the camera along a smooth path through them, one frame per yield, until the tilt track
 * finishes. */
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
    script = (u8*)thread_get_current_parameter_1();
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
static s32 camera_step_track(camera_track_t* track, s32 component) {
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
static s32 script_interpolate_range_fixed12(s32 start, s32 end, s32 fraction, s32 value) {
    return mul_div_s64(end - start, fraction * ONE, value) + (start * ONE);
}

/* DebugPause event instruction (0x72): print the pause message with the debug font, yield once and
 * set g_script_debug_paused, which nothing reads. */
void script_print_debug_message(void) {
    FntPrint(g_script_pause_message);
    thread_yield();
    g_script_debug_paused = 1;
}

/* Start movement to the encoded tile with the signed speed and terrain-cost flag. */
void script_opcode_walk_to(const event_walk_to_parameters_t* parameters) {
    s32 id = script_read_s16(parameters->unit_id_le);
    map_tile_coordinates_t coordinates;

    id = script_resolve_runtime_id(id);
    if (id != EVENT_RUNTIME_ID_NONE) {
        coordinates.x = parameters->x;
        coordinates.y = parameters->y;
        coordinates.layer = parameters->elevation;
        battle_move_start_unit_walk_to(id, &coordinates,
            MOVE_PATHFIND_IGNORE_TERRAIN_COST - parameters->terrain_cost_flag * MOVE_PATHFIND_IGNORE_TERRAIN_COST, 3,
            script_read_s16(parameters->speed_le));
    }
}

/* Mirror the unit sprite for mode 1 and restore it for every other mode. */
void script_opcode_mirror_sprite(const event_mirror_sprite_parameters_t* parameters) {
    s32 mode;
    s32 runtime_id = script_read_s16(parameters->unit_id_le);

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
void script_opcode_reset_palette(const event_unit_parameters_t* parameters) {
    s32 runtime_id = script_resolve_runtime_id(script_read_s16(parameters->unit_id_le));
    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        battle_gfx_reset_unit_palette_by_runtime_id(runtime_id);
    }
}

/* Jump: cancel the unit's accumulated event offset, then build its one-step path
 * of length_plus_one - 1 tiles in the encoded direction through
 * battle_move_start_unit_jump_by_runtime_id. The unit's Jump stat is 6 while that runs
 * and is restored afterwards. A unit id that does not resolve does nothing. */
void script_opcode_jump(const event_jump_parameters_t* parameters) {
    SVECTOR vec;
    s32 runtime_id = script_resolve_runtime_id(script_read_s16(parameters->unit_id_le));
    SVECTOR* coords;
    unit_stats_t* stats;
    u8 saved;

    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        coords = battle_unit_get_event_offset_by_runtime_id(runtime_id);
        setVector(&vec, -coords->vx, -coords->vy, -coords->vz);
        battle_unit_add_event_offset_by_runtime_id(runtime_id, &vec);
        stats = battle_unit_get_stats_from_battle_id(battle_unit_get_battle_id_by_runtime_id(runtime_id));
        saved = stats->jump;
        stats->jump = 6;
        battle_move_start_unit_jump_by_runtime_id(runtime_id, parameters->direction, parameters->length_plus_one - 1);
        stats->jump = saved;
    }
}

/* Yield at least once, then wait until the resolved unit stops moving. */
void script_opcode_wait_walk(s32 unit_id) {
    s32 runtime_id = script_resolve_runtime_id(unit_id);

    if (runtime_id != EVENT_RUNTIME_ID_NONE) {
        do {
            thread_yield();
        } while (battle_move_is_unit_moving_by_runtime_id(runtime_id) != 0);
    }
}

/* Preload ATTACK.OUT when EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS is clear:
 * load companion executable 0xd in preload mode, look up the pending
 * scenario's special portrait and unit, and build the formation sprites and
 * portraits, so the later ATTACK entry skips reloading the overlay, sprites
 * and portraits. */
void script_opcode_load_attack_graphics(void) {
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
    g_thread_call_target = battle_effect_call_play;
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
    script = (u8*)thread_get_current_parameter_1();
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
            script_opcode_wait_value((const event_wait_value_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_DEBUG_PAUSE) {
            script_print_debug_message();
        } else if (script[0] == EVENT_OPCODE_WALK_TO) {
            g_thread_call_target = (void (*)(void))script_opcode_walk_to;
            thread_call_on_main_stack(parameters);
        } else if (script[0] == EVENT_OPCODE_WALK_TO_ANIM) {
            runtime_id = script_resolve_runtime_id(first_halfword);
            if (runtime_id != EVENT_RUNTIME_ID_NONE) {
                g_unit_pending_animation_ids[runtime_id] = script_read_s16(script + 3);
            }
            g_unit_was_moving_latch[runtime_id] = 0;
        } else if (script[0] == EVENT_OPCODE_JUMP) {
            script_opcode_jump((const event_jump_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_WAIT_WALK) {
            script_opcode_wait_walk(first_halfword);
        } else if (script[0] == EVENT_OPCODE_UNIT_ANIM) {
            script_unit_anim((const event_unit_anim_parameters_t*)parameters);
        } else if (script[0] == EVENT_OPCODE_MIRROR_SPRITE) {
            script_opcode_mirror_sprite((const event_mirror_sprite_parameters_t*)(script + 1));
        } else if (script[0] == EVENT_OPCODE_RESET_PALETTE) {
            script_opcode_reset_palette((const event_unit_parameters_t*)(script + 1));
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
            g_thread_contexts[thread_id].task_words[SPRITE_MOVE_THREAD_WORD_RUNTIME_ID]
                = script_resolve_runtime_id(first_halfword);
        } else if (script[0] == EVENT_OPCODE_WAIT_SPRITE_MOVE) {
            script_opcode_wait_sprite_move(first_halfword);
        } else if (script[0] == EVENT_OPCODE_WAIT_ROTATE_UNIT) {
            script_opcode_wait_rotate_all(script[1]);
        } else if (script[0] == EVENT_OPCODE_WAIT_ROTATE_ALL) {
            script_opcode_wait_rotate_all(-1);
        } else if (script[0] == EVENT_OPCODE_UNIT_ANIM_ROTATE) {
            script_opcode_unit_anim_rotate((const event_unit_anim_rotate_parameters_t*)(script + 1));
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
    script = (const u8*)thread_get_current_parameter_1();
    while (1) {
        unit_id = script_read_s16(script + 1);
        opcode = script[0];
        flag = script[3];
        if (opcode == EVENT_OPCODE_ADD_UNIT_END) {
            break;
        }
        if (opcode == EVENT_OPCODE_LOAD_ATTACK_GRAPHICS) {
            script_opcode_load_attack_graphics();
        } else if (opcode == EVENT_OPCODE_ADD_UNIT) {
            unit = battle_unit_find_stats_by_entd_unit_id(unit_id, &index);
            if (index == -2 || index >= 0) {
                for (slot = 0; slot < BATTLE_UNIT_SLOT_COUNT; slot++) {
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

/* Reset the menu library for a battle's setup: subsystem pointers, palette and text, the
 * menu subsystems, the script light transform and the panel fade tiles. */
static void menu_init_subsystems(void) {
    menu_init_subsystem_pointers();
    menu_init_palette_and_text(0);
    menu_init_stub();
    menu_reset_subsystems();
    map_reset_script_light_transform();
    menu_init_panel_fade_tiles();
}

/* Bind the scene working pointers and clear its menu-session flags. */
void menu_init_subsystem_pointers(void) {
    g_threads = g_thread_contexts;
    g_text_glyph_bitmap_data = g_text_glyph_bitmaps;
    g_menu_glyph_sheet = &g_menu_glyph_image_data;
    g_script_event_block = g_main_script_event_block;
    g_script_variables = g_main_script_variables;
    g_menu_thread_menu_data = g_menu_idle_action_entries;
    g_menu_help_active_text_id_tables = (u16* const*)-1;
    g_unit_graphics_load_pending = 0;
    g_script_threads_idle = 0;
    g_script_frame_result_override = 0;
}

/* Reset what the menu layer shares: reseed the event script's random generator,
 * free every window buffer pair, the tpage-7 VRAM grid and the menu buffer arena,
 * and reset the thread scheduler. */
void menu_reset_subsystems(void) {
    script_seed_random_from_vsync();
    menu_reset_window_buffer_slots();
    gfx_clear_texture_allocation_grid();
    menu_init_buffer_allocator();
    thread_reset_scheduler();
}

/* Put the map light transform of the event scripts back to the map's own light: no rotation, unit
 * scale, and the current light direction and color matrices as the base map_light_thread
 * interpolates from. WORLD has no map light, so its copy is empty. */
void map_reset_script_light_transform(void) {
#ifdef FFT_BATTLE
    MATRIX* buffer;

    setVector(&g_script_light_angles, 0, 0, 0);
    setVector(&g_script_light_scale, ONE, ONE, ONE);
    buffer = battle_map_light_matrix_command(MAP_LIGHT_MATRIX_COMMAND_GET_LIGHT, buffer, buffer, 0);
    mem_copy_bytes_nonempty(
        &g_script_light_base_direction_matrix, buffer, sizeof(g_script_light_base_direction_matrix));
    mem_copy_bytes_nonempty(&g_script_light_base_color_matrix,
        battle_map_light_matrix_command(MAP_LIGHT_MATRIX_COMMAND_GET_COLOR, buffer, buffer, 0),
        sizeof(g_script_light_base_color_matrix));
#endif
}

/* Stop every unit's event rotation and pending animation, and record each present unit's
 * facing as its initial facing (0xff for an empty slot). */
static void unit_reset_animation_states(void) {
    unit_animation_state_t* state;
    s32 i = 0;

    state = g_unit_animation_states;
    for (; i < BATTLE_UNIT_SLOT_COUNT; i++, state++) {
        state->rotating = 0;
        state->delay = 0;
        if (battle_unit_has_runtime_id(i) != 0) {
            state->initial_facing = battle_unit_get_facing_nibble_by_runtime_id(i);
        } else {
            state->initial_facing = 0xff;
        }
        g_unit_pending_animation_ids[i] = 0;
        g_unit_was_moving_latch[i] = 0;
    }
}

/* Per frame, apply each unit's pending animation on the frame it stops moving
 * and step each rotating unit one facing (of 16) toward its target, after its
 * delay and once every frames_per_step frames. The shortest mode turns
 * whichever way is nearer; the other two modes always increment or decrement.
 * Nothing calls it in WORLD, where its caller script_process_pending_requests
 * is empty. */
void unit_update_facing_and_pending_animations(void) {
    s32 i;
    s32 facing;
    s32 current;
    s32 next;
    s32 moving;
    unit_animation_state_t* state = g_unit_animation_states;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++, state++) {
        if (battle_unit_has_runtime_id(i) != 0) {
            if (g_unit_pending_animation_ids[i] != 0) {
                moving = battle_move_is_unit_moving_by_runtime_id(i);
                if (moving == 0 && g_unit_was_moving_latch[i] != 0) {
                    unit_set_animation(i, g_unit_pending_animation_ids[i]);
                    g_unit_pending_animation_ids[i] = 0;
                }
                g_unit_was_moving_latch[i] = moving;
            }
            current = battle_unit_get_facing_nibble_by_runtime_id(i);
            if (state->rotating == 0) {
                continue;
            }
            if (current == state->target_facing) {
                state->rotating = 0;
                continue;
            }
            state->step_counter++;
            if (state->delay != 0) {
                state->delay--;
                continue;
            }
            if (state->step_counter < state->frames_per_step) {
                continue;
            }
            state->step_counter = 0;
            if (state->rotate_mode == UNIT_ROTATE_MODE_SHORTEST) {
                moving = state->target_facing - current + 0x10;
                facing = current - state->target_facing + 0x10;
                moving &= 0xF;
                facing &= 0xF;
                if (facing < moving) {
                    next = current + 0xF;
                } else {
                    next = current + 1;
                }
                facing = next & 0xF;
            } else if (state->rotate_mode == UNIT_ROTATE_MODE_INCREMENT) {
                facing = (current + 1) & 0xF;
            } else if (state->rotate_mode == UNIT_ROTATE_MODE_DECREMENT) {
                facing = (current + 0xF) & 0xF;
            }
            unit_set_facing(i, facing);
        } else {
            g_unit_was_moving_latch[i] = 0;
            g_unit_pending_animation_ids[i] = 0;
        }
    }
}

/* Reset the unit animation states and, when the current-event variable is set,
 * fade the screen color modulation to zero and start the event thread (slot 1)
 * on script_run_next_event.
 *
 * While the battle has not started it first stores the camera in the script
 * variables and resets the script light transform. Where WORLD's copy calls two
 * empty functions, BATTLE does that camera work. */
void script_start_current_event_thread(void) {
    unit_reset_animation_states();
    g_script_attack_entry_mode = SCRIPT_ATTACK_ENTRY_NORMAL;
    if (script_get_variable(EVENT_SCRIPT_VAR_BATTLE_NOT_STARTED) != 0) {
        camera_store_state_to_script_variables();
        map_reset_script_light_transform();
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
    }
    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0) {
        g_camera_position_locked = 0;
        g_camera_speed_curve = 0;
        battle_gfx_start_screen_color_modulation_fade(2, 0, 0, 0, 2);
        thread_start(THREAD_SLOT_EVENT, script_run_next_event);
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_start_current_event_thread and script_reset_event_state. Nothing on
 * the disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_8(void) {
}

/* Reset unit animation state; while the event slot is idle, run the scene
 * camera/light reset hooks and clear camera interpolation and position lock. */
void script_reset_event_state(void) {
    unit_reset_animation_states();
    if (thread_is_running_asm(THREAD_SLOT_EVENT) == 0) {
        camera_store_state_to_script_variables();
        map_reset_script_light_transform();
        g_camera_speed_curve = 0;
        g_camera_position_locked = 0;
    }
}

/* Run one event frame, then return whether the deployment thread (borrowed slot
 * 6) is still running. */
s32 script_is_deployment_running(u32* ot, u32 buttons) {
    script_run_event_frame_variant(ot, buttons);
    return thread_is_running(THREAD_BORROWED_SLOT_DEPLOYMENT);
}

/* Run one menu and event frame and return the resolved menu selection. It records the cursor
 * tile's info, clears the pending selections, and forces event speed 2 while a command menu other
 * than the mini menu is open, restoring the saved speed afterwards. Without player input it uses
 * the unfiltered script input. */
s32 menu_update_menus_and_event_speed(u32* otag, s32 controller_input) {
    s32 i;
    s32 result;
    s32 fill;

    g_ai_workspace_ptr = (event_work_t*)g_ai_workspace;
    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        controller_input = g_script_unfiltered_controller_input;
    }
    map_store_selected_tile_data();
    fill = -2;
    for (i = 3; i >= 0; i--) {
        g_menu_pending_selection[i] = fill;
    }
    script_run_event_frame_variant(otag, controller_input);
    menu_update_panel_fade();
    result = menu_resolve_selection();
    if ((thread_is_running_asm(THREAD_SLOT_MENU) != 0
            && g_thread_contexts[THREAD_SLOT_MENU].task_id != THREAD_TASK_MINI_MENU)
        || thread_is_running_asm(THREAD_SLOT_MENU_CHILD) != 0
        || thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0) {
        if (g_menu_animation_speed_forced == 0) {
            g_menu_animation_speed_forced = 1;
            g_menu_saved_animation_speed = battle_state_get_vsync_interval();
            script_set_event_speed(2);
        }
    } else if (g_menu_animation_speed_forced == 1) {
        g_menu_animation_speed_forced = 0;
        script_set_event_speed(g_menu_saved_animation_speed);
    }
    return result;
}

/* Fill g_map_selected_tile_data for the tile under the cursor, which the
 * tile information display shows: the surface type, the depth, the displayed
 * height in half-height units (height * 2 + slope half-height + depth * 2) and
 * the surface type's two bytes of g_main_terrain_tile_info_pairs. */
static void map_store_selected_tile_data(void) {
    SVECTOR coords;
    s16* surface_type;
    s32 depth;

    battle_map_store_selected_tile_coordinates(&coords);
    surface_type = &g_map_selected_tile_data.surface_type;
    *surface_type = battle_map_get_tile_data_value(MAP_TILE_DATA_SURFACE_TYPE, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.display_height
        = battle_map_get_tile_data_value(MAP_TILE_DATA_HALF_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.display_height
        += (battle_map_get_tile_data_value(MAP_TILE_DATA_HEIGHT, coords.vx, coords.vz, coords.vy) & 0xFF) * 2;
    depth = battle_map_get_tile_data_value(MAP_TILE_DATA_DEPTH, coords.vx, coords.vz, coords.vy) & 0xFF;
    g_map_selected_tile_data.depth = depth;
    g_map_selected_tile_data.display_height += depth * 2;
    g_map_selected_tile_data._unknown_02 = g_main_terrain_tile_info_pairs[*surface_type][0];
    g_map_selected_tile_data._unknown_04 = g_main_terrain_tile_info_pairs[*surface_type][1];
}

/* Reset the cursor row of the 11 menu entries listed in g_menu_cleared_entry_indices. */
void menu_clear_entry_selections(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        g_menu_thread_menu_data[g_menu_cleared_entry_indices[i]].selected_index = 0;
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_clear_entry_selections and menu_restore_selection_for_selected_unit.
 * Nothing on the disc calls it or points at it; the name is a neutral
 * placeholder until a reference turns up. */
void menu_noop_7(void) {
}

/* Restore the selected unit's remembered command row, applying menu remaps. */
void menu_restore_selection_for_selected_unit(s32 menu_id) {
    menu_unit_selection_record_t* record;
    s32 i;
    s32 row;
    s32 candidate;
    menu_selection_remap_t* remap;

    for (i = 0; i < 12; i++) {
        if (g_menu_command_maps[i].menu_id == menu_id) {
            break;
        }
    }
    if (i == 12) {
        return;
    }
    record = &g_menu_unit_selection_records[g_menu_selected_unit.battle_id];
    for (row = 0, remap = g_menu_selection_remaps; row < 8; remap++, row++) {
        if (menu_id == g_menu_selection_remaps[row].menu_id) {
            /* Keep the signed byte-table index used by the target. */
            for (candidate = 2; candidate < 5; candidate++) {
                if (record->command_index == remap->matches[candidate - 2]) {
                    record->command_index = g_menu_selection_remaps[row].replacement;
                    break;
                }
            }
        }
    }
    g_menu_thread_menu_data[menu_id].selected_index = record->command_index;
}

/* One row per battle unit, each 17 bytes. A typed row-array version changes the target's
 * induction variables (84 bytes rather than 76), so retain the byte view.
 * Use an integer countdown: decrementing an element pointer after row[0]
 * would form a pointer before the first row. */
enum { WORLD_MENU_UNIT_SELECTION_RECORD_BYTES = (s32)sizeof(menu_unit_selection_record_t) };

/* Reset the per-unit menu records, clearing each and marking byte 1 as
 * unset (0xFF). */
void menu_reset_unit_records(void) {
    s32 index;
    u8* row;
    s32 count;
    u8 fill = 0xFF;

    index = 0;
    row = (u8*)g_menu_unit_selection_records;
    do {
        count = WORLD_MENU_UNIT_SELECTION_RECORD_BYTES - 1;
        do {
            row[count] = 0;
            count -= 1;
        } while (count >= 0);
        ((u8*)g_menu_unit_selection_records)[index + 1] = fill;
        index += WORLD_MENU_UNIT_SELECTION_RECORD_BYTES;
        row += WORLD_MENU_UNIT_SELECTION_RECORD_BYTES;
    } while (index < BATTLE_UNIT_SLOT_COUNT * WORLD_MENU_UNIT_SELECTION_RECORD_BYTES);
}

/* Clear one unit's remembered menu rows, as menu_reset_unit_records does for all (byte 1 back to
 * 0xff), but only for a unit under manual control. The test sits inside the byte loop. */
void menu_reset_unit_record(s32 battle_id) {
    unit_stats_t* stats = battle_unit_get_stats_from_battle_id(battle_id);
    menu_unit_selection_record_t* records = g_menu_unit_selection_records;
    s32 i;

    for (i = 0; i < 17; i++) {
        if (stats->auto_battle_setting == AI_AUTOBATTLE_MANUAL) {
            ((u8*)&records[battle_id])[i] = 0;
            g_menu_unit_selection_records[battle_id]._unknown_01 = 0xFF;
        }
    }
}

/* The command record the menus fill when the player picks a command (g_menu_selected_command). */
ai_command_t* menu_get_selected_command_address(void) {
    return &g_menu_selected_command;
}

/* Post the close command after two frames under the mini-menu task id. */
void menu_mini_menu_cancel_delay_thread(void) {
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
    thread_wait_frames(2);
    g_menu_pending_selection[0] = 7;
    thread_exit_current();
}

/* Queue the confirm sound and hold the mini-menu task id for 16 frames. */
void menu_confirm_sound_delay_thread(void) {
    g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
    thread_wait_frames(16);
    thread_exit_current();
}

/* Widen bytes into 16-bit menu values up to a 0xff terminator, which is not copied, and return the
 * count. */
static s32 menu_copy_u8_to_u16_terminated(u16* destination, const u8* source) {
    s32 count = 0;

    for (;;) {
        if (*source == 0xFF) {
            return count;
        }
        *destination = *source;
        source++;
        count++;
        destination++;
    }
}

/* Widen count bytes into 16-bit menu values; a nonpositive count writes nothing. */
static void menu_copy_u8_to_u16(u16* destination, const u8* source, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        *destination = *source;
        source++;
        destination++;
    }
}

/* Builds the ability list for the active skillset menu entry (menu entry 4's
 * selected_index selects the skillset; g_action_menu_row_types gives its list type). Each type
 * fills the 0x52-row work buffer at *g_ai_workspace_ptr through its own loader, then
 * the rows are sized, grayed (not enough MP, or silenced for voice abilities)
 * and the window layout at 0x80166994 is set up for 6 visible rows.
 *
 * Matching notes: the layout block is one struct (its x/y are s16, so `-= 4`
 * stays an addiu immediate and the constant is not shared); window_x is a
 * separate local from the unit index, and its updates follow the loader
 * calls in each arm. */
static void menu_build_ability_list(s32 mode) {
    s32 unit_index;
    s32 window_x;
    unit_stats_t* unit;
    s32 i;
    s32 type;
    s32 skillset;
    s32 count;
    s32 found;
    s32 columns;
    s32 row_mode;
    s32 row_flags;

    g_menu_ability_layout.columns.text_ids[0] = (s16*)g_ai_workspace_ptr->ability_list.ids;
    g_menu_ability_layout.columns.text_ids[1] = (s16*)g_ai_workspace_ptr->ability_list.values;
    g_menu_ability_layout.columns.text_ids[2] = (s16*)g_ai_workspace_ptr->ability_list.extras;
    unit_index = g_menu_selected_unit.battle_id;
    unit = menu_get_selected_unit_stats();
    type = g_action_menu_row_types[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index];
    skillset = g_menu_skillsets[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index];
    g_menu_selected_action.skillset_id = skillset;
    for (i = 0; i < 80; i++) {
        g_ai_workspace_ptr->ability_list.ids[i] = 0xFFFF;
        g_ai_workspace_ptr->ability_list.mp_costs[i] = 0xFF;
        g_ai_workspace_ptr->ability_list.ct[i] = 1;
        g_ai_workspace_ptr->ability_list.values[i] = 0;
        g_ai_workspace_ptr->ability_list.extras[i] = 0xFFFF;
        g_ai_workspace_ptr->ability_list.flags[i] = 0;
        g_ai_workspace_ptr->ability_list.turns[i] = 1;
        g_ai_workspace_ptr->ability_list.skill_ct[i] = 0xFF;
        g_menu_ability_display_flags[i] = 0;
    }
    g_ai_workspace_ptr->ability_list.mp_costs[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.ct[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.flags[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.turns[0x50] = 0xFF;
    g_ai_workspace_ptr->ability_list.skill_ct[0x50] = 0xFF;

    if (type == ACTION_MENU_TYPE_DEFAULT) {
        battle_menu_get_unit_skillset_ability_data(unit_index, skillset, (s16*)g_ai_workspace_ptr->ability_list.ids,
            g_ai_workspace_ptr->ability_list.mp_costs, g_ai_workspace_ptr->ability_list.ct, 0,
            g_ai_workspace_ptr->ability_list.flags, g_ai_workspace_ptr->ability_list.turns);
        mem_copy_bytes_nonempty(g_ai_workspace_ptr->ability_list.skill_ct, g_ai_workspace_ptr->ability_list.ct, 0x50);
    }
    if (type == ACTION_MENU_TYPE_ITEM_INVENTORY) {
        battle_menu_display_item_inventory_ability(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_WEAPON_INVENTORY) {
        battle_menu_load_throw_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_ARITHMETICKS) {
        if (mode == MENU_ARITHMETICKS_LIST_ATTRIBUTES) {
            battle_menu_load_math_skill_attributes(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
        if (mode == MENU_ARITHMETICKS_LIST_MULTIPLES) {
            battle_menu_load_math_skill_multiples(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
        if (mode == MENU_ARITHMETICKS_LIST_ABILITIES) {
            battle_menu_collect_calculator_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids);
        }
    }
    if (type == ACTION_MENU_TYPE_MONSTER) {
        battle_menu_collect_monster_skill_abilities(
            unit_index, skillset, g_ai_workspace_ptr->ability_list.ids, 0, g_ai_workspace_ptr->ability_list.flags);
    }
    if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
        battle_menu_load_draw_out_abilities(unit_index, skillset, g_ai_workspace_ptr->ability_list.mp_costs);
    }
    if (type == ACTION_MENU_TYPE_CHARGE) {
        battle_menu_load_charge_skillset(unit_index, skillset, g_ai_workspace_ptr->ability_list.ids,
            g_ai_workspace_ptr->ability_list.mp_costs, g_ai_workspace_ptr->ability_list.turns);
        mem_copy_bytes_nonempty(
            g_ai_workspace_ptr->ability_list.skill_ct, g_ai_workspace_ptr->ability_list.mp_costs, 0x50);
    }
    window_x = 0xAC;
    if (type != ACTION_MENU_TYPE_ARITHMETICKS) {
        mode = MENU_ARITHMETICKS_LIST_ABILITIES;
    }

    g_menu_ability_layout.columns.mode[1] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_menu_ability_layout.columns.x[1] = 0x6A;
    g_menu_ability_layout.columns.x[2] = 0x7E;
    g_menu_ability_layout.columns.mode[0] = MENU_TEXT_COLUMN_TEXT;
    if (g_menu_action_type_modes[type][1] == 1) { /* lists by id */
        count = menu_copy_u8_to_u16_terminated(
            g_ai_workspace_ptr->ability_list.ids, g_ai_workspace_ptr->ability_list.mp_costs);
        menu_copy_u8_to_u16(g_ai_workspace_ptr->ability_list.extras, g_ai_workspace_ptr->ability_list.turns, count);
        found = 0;
        for (i = 0; i < count; i++) {
            g_ai_workspace_ptr->ability_list.values[i]
                = g_main_item_quantities[g_ai_workspace_ptr->ability_list.ids[i]];
            if (g_ai_workspace_ptr->ability_list.values[i] == 0) {
                g_menu_ability_display_flags[i] = 4;
            } else {
                found = 1;
            }
        }
        row_mode = 7;
        if (found != 0) {
            window_x -= 4;
        } else {
            g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
            menu_copy_u8_to_u16(g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.turns, count);
            window_x = 0x94;
            row_mode = 8;
        }
        row_flags = TEXT_ID_ITEM_NAME_BASE;
        g_menu_ability_layout.columns.x[1] -= 4;
        g_menu_ability_layout.columns.x[2] -= 4;
        columns = 0x11;
        if (type == ACTION_MENU_TYPE_KATANA_INVENTORY) {
            columns = 0x17;
        }
    } else {
        found = 0;
        for (count = 0; count < 64; count++) {
            if (g_ai_workspace_ptr->ability_list.ids[count] == 0xFFFF) {
                break;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT) {
            for (i = 0; i < count; i++) {
                if (g_ai_workspace_ptr->ability_list.mp_costs[i] != 0) {
                    found = 1;
                    break;
                }
            }
        }
        if (type == ACTION_MENU_TYPE_ARITHMETICKS && mode > 0) {
            window_x = 0x88;
            g_menu_ability_layout.columns.mode[1] = MENU_TEXT_COLUMN_HIDDEN;
            g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
            row_mode = 9;
        } else {
            if (found != 0 && type == ACTION_MENU_TYPE_DEFAULT) {
                menu_copy_u8_to_u16_terminated(
                    g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.mp_costs);
                window_x -= 8;
                menu_copy_u8_to_u16(
                    g_ai_workspace_ptr->ability_list.extras, g_ai_workspace_ptr->ability_list.turns, count);
                row_mode = 6;
                g_menu_ability_layout.columns.x[1] -= 8;
                g_menu_ability_layout.columns.x[2] -= 8;
            } else {
                menu_copy_u8_to_u16(
                    g_ai_workspace_ptr->ability_list.values, g_ai_workspace_ptr->ability_list.turns, count);
                window_x = 0x94;
                row_mode = 5;
                g_menu_ability_layout.columns.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
                g_menu_ability_layout.columns.x[1] -= 4;
                g_menu_ability_layout.columns.x[2] -= 4;
            }
        }
        row_flags = TEXT_ID_ABILITY_NAME_BASE;
        columns = 5;
    }

    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].inner_width = window_x;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].window.w = window_x;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].text_image.width = window_x;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].header_id = row_mode;
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].select_text_table = columns;
    for (i = 0; i < count; i++) {
        g_ai_workspace_ptr->ability_list.ids[i] |= row_flags;
        if (type == ACTION_MENU_TYPE_DEFAULT) {
            if (unit->mp - g_ai_workspace_ptr->ability_list.mp_costs[i] < 0
                && g_ai_workspace_ptr->ability_list.skill_ct[i] != 0) {
                g_menu_ability_display_flags[i] = 8;
            }
        }
        if (type == ACTION_MENU_TYPE_DEFAULT || type == ACTION_MENU_TYPE_MONSTER) {
            if ((g_ai_workspace_ptr->ability_list.flags[i] & 2) && g_ai_workspace_ptr->ability_list.skill_ct[i] != 0
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_SILENCE)]
                    & STATUS_BYTE_MASK(STATUS_ID_SILENCE))) {
                g_menu_ability_display_flags[i] = 8;
            }
        }
    }
    if (count == 0) {
        count = 1;
        g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].header_id = 0;
        g_ai_workspace_ptr->ability_list.ids[0] = TEXT_ID_ABILITY_NAME_BASE;
        g_ai_workspace_ptr->ability_list.values[0] = 0;
        g_ai_workspace_ptr->ability_list.extras[0] = 0;
        g_menu_ability_display_flags[0] = 4;
    }
    g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].window.y = 0x30;
    if (count >= 7) {
        g_menu_ability_layout.columns.hidden_rows = count - 6;
        g_menu_ability_layout.columns.row_count = 6;
    } else {
        g_menu_ability_layout.columns.hidden_rows = 0;
        g_menu_ability_layout.columns.row_count = count;
        g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].window.y += (6 - count) * 8;
    }
    /* The menu thread's redraw, cursor and refresh parameters. */
    g_thread_contexts[g_thread_current_id].function_parameter_2 = 1;
    g_thread_contexts[g_thread_current_id].function_parameter_3 = 0;
    g_thread_contexts[g_thread_current_id].function_parameter_4 = 1;
}

/* Menu entry 4's value and the loop counter share $s0 in the target: the
 * index dies after the two table loads and the register is reused by the
 * `sll s0,v0,0x1` that forms the counter, so they are one variable here.
 * g_menu_action_slot_row_actions is written as an in-loop expression rather than a pre-loop
 * pointer local so that loop.c hoists it into the preheader after the bltz,
 * which is where the target's `lui/addiu s8` pair sits. */

/*
 * Walk the stored unit's
 * menu record slots from the top value down to zero, presenting each through
 * the inner subroutine and writing the chosen entry back into the record.
 */
void menu_select_unit_action_slots_thread(void) {
    s32 i = g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index;
    s32 menu = g_action_menu_row_types[i];
    s32 check_unit = g_battle_menu_skillset_disabled_flags[i];
    menu_unit_selection_record_t* row = &g_menu_unit_selection_records[g_menu_selected_unit.battle_id];
    menu_list_selection_t* slot;
    u8* pair;
    s32 j;
    s16 text;

    /* Typed list indexing changes the address order; these byte views retain it. */
    for (i = (g_menu_restore_pending == 0) ? 2 : 0; i >= 0; i--) {
        menu_check_action_slot_restrictions(menu, i, check_unit);
        if (menu == ACTION_MENU_TYPE_ARITHMETICKS) {
            slot = (menu_list_selection_t*)((u8*)row + (i + offsetof(menu_unit_selection_record_t, lists[4])) + i);
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index = slot->selected_index;
            g_menu_ability_layout.row_offset = slot->scroll_offset;
        } else {
            pair = (u8*)row
                + g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index * sizeof(menu_list_selection_t);
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index
                = pair[offsetof(menu_unit_selection_record_t, lists[0].selected_index)];
            g_menu_ability_layout.row_offset = pair[offsetof(menu_unit_selection_record_t, lists[0].scroll_offset)];
            slot = (menu_list_selection_t*)(pair + offsetof(menu_unit_selection_record_t, lists));
        }
        for (j = 0; j < 6; j++) {
            if (menu != ACTION_MENU_TYPE_ARITHMETICKS || i == 0) {
                g_menu_action_slot_row_actions[j] = 0x1024;
            } else {
                g_menu_action_slot_row_actions[j] = -1;
            }
        }
        if (g_action_menu_row_types[g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index]
            == ACTION_MENU_TYPE_CHANGE_EQUIPMENT) {
            g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index = 0;
            menu_run_equip_thread();
        }
        g_thread_call_target = menu_build_ability_list;
        thread_call_on_main_stack(i);
        script_pulse_tutorial_wait_value(0xFB);
        menu_run_scrolling_ability_list_thread_variant();
        slot->selected_index = g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index;
        slot->scroll_offset = g_menu_ability_layout.row_offset;
        if (menu != ACTION_MENU_TYPE_ARITHMETICKS) {
            thread_exit_current();
        }
        if (g_menu_action_slot_selected_option == -1) {
            if (i == 2) {
                thread_exit_current();
            }
            i += 2;
        } else {
            text = g_ai_workspace_ptr->ability_list
                       .ids[g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST].selected_index]
                - 0x7000;
            if (i == 2) {
                g_menu_selected_action.calculator_type = text;
            }
            if (i == 1) {
                g_menu_selected_action.calculator_multiplier = text;
            }
            if (i == 0) {
                g_menu_selected_action.ability_id = text;
            }
        }
    }
    thread_exit_current();
}

/* Build skillset selection rows and size the selected unit's menu. */
static void menu_build_skillset_entries(void) {
    menu_text_row_t* rows;
    s32 count = 0;
    s32 i;
    s16 height;
    u8 separator;

    g_menu_selected_skill_ct = 0;
    g_menu_skillsets[0] = 0;
    g_menu_skillset_build_result = battle_menu_build_unit_action_menus(g_menu_selected_unit.battle_id, g_menu_skillsets,
        g_battle_menu_skillset_disabled_flags, g_action_menu_row_types);
    rows = (menu_text_row_t*)text_find_entry(0x5010);
    if (battle_unit_get_action_restriction(menu_get_selected_unit_stats())
        == BATTLE_UNIT_ACTION_RESTRICTION_DEEP_WATER) {
        for (i = 0; i < 6; i++) {
            g_battle_menu_skillset_disabled_flags[i] = 1;
            if (g_action_menu_row_types[i] == ACTION_MENU_TYPE_UNKNOWN_0D) {
                g_battle_menu_skillset_disabled_flags[i] = 0;
            }
        }
    }
    /* The separator byte is materialized before the row counter in the
     * target; a hoisted constant lands after the counter's initializer. */
    separator = 0xfa;
    for (i = 5; i >= 0; i--) {
        rows[i].terminator = separator;
    }
    for (i = 0; i < 6; i++) {
        rows[i].terminator = 0xfa;
        if (g_menu_skillsets[i] == SKILLSET_ID_NONE) {
            break;
        }
        if (g_battle_menu_skillset_disabled_flags[i] != 0) {
            rows[i].style = 4;
            g_menu_skillset_row_actions[i] = 0x1003;
        } else {
            rows[i].style = 0;
            g_menu_skillset_row_actions[i] = 3;
        }
        if (g_menu_skillset_build_result != 0) {
            g_text_substitution_values[count] = TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
        } else {
            g_text_substitution_values[count] = g_menu_skillsets[i] + TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
        }
        count++;
    }
    if (script_get_variable(EVENT_SCRIPT_VAR_ANYTHING_ACTION_ENABLED) != 0 && i < 6) {
        rows[i].style = 0;
        g_menu_skillset_row_actions[i] = 0x16;
        g_menu_skillsets[i] = ACTION_MENU_PSEUDO_SKILLSET_ANYTHING;
        g_menu_skillsets[i + 1] = SKILLSET_ID_NONE;
        g_text_substitution_values[count++] = TEXT_ID_SKILLSET_NAME_SOURCE_BASE + ACTION_MENU_PSEUDO_SKILLSET_ANYTHING;
    }
    height = count * 16 + 16;
    rows[count - 1].terminator = 0xff;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].max_row_index = count - 1;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].text_image.height = height;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].window.h = height;
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].inner_height = height;
}

/* Build the skillset list, then wait for its submenu or run icon selection.
 * The completion value written to the thread's
 * second parameter is materialized before the branch, which puts `li v1,1`
 * in the beqz delay slot ahead of the 0xFC argument. */
void menu_run_skillset_thread(void) {
    s32 param_value;
    menu_unit_selection_record_t* row;
    s32* param;

    menu_check_action_restrictions();
    g_thread_call_target = menu_build_skillset_entries;
    thread_call_on_main_stack();
    param_value = 1;
    if (g_menu_skillset_build_result != 0) {
        g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index = 0;
        if (g_menu_restore_pending == 0) {
            thread_start(g_thread_current_id - 1, menu_select_unit_action_slots_thread);
            thread_set_parameters(
                g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST], 0, 0);
        } else {
            thread_wait_frames(2);
        }
        for (;;) {
            thread_yield();
            if (thread_is_running_asm(g_thread_current_id - 1) == 0) {
                thread_exit_current();
            }
        }
    }
    param = &g_thread_contexts[g_thread_current_id].function_parameter_2;
    *param = param_value;
    row = &g_menu_unit_selection_records[g_menu_selected_unit.battle_id];
    g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index = row->skillset_index;
    g_menu_restore_depth = 1;
    script_pulse_tutorial_wait_value(0xFC);
    menu_run_icon_selection_loop();
    row->skillset_index = g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST].selected_index;
    thread_exit_current();
}

/* Record the current menu's selection byte for the selected unit, when that
 * menu appears in the tracked menu-id table. */
void menu_record_selection_for_selected_unit(void) {
    menu_unit_selection_record_t* records;
    s16* selected = &g_menu_selected_unit.battle_id;
    s32 i;

    records = g_menu_unit_selection_records;
    for (i = 0; i < 12; i++) {
        if (g_menu_current_id == g_menu_command_maps[i].menu_id) {
            records[*selected].command_index = g_menu_thread_menu_data[g_menu_current_id].selected_index;
        }
    }
}

/* Thread entry of the turn menus (entries 0x00, 0x0e, 0x13, 0x14, 0x20, 0x21 and
 * 0x2e-0x33 of the idle action menu table): run the icon selection loop, then
 * remember the chosen row for the active-turn unit and exit. */
void menu_build_main(void) {
    menu_entry_t* parameter = (menu_entry_t*)thread_get_current_parameter_1();
    menu_unit_selection_record_t* record;
    s32 unit_id = g_menu_selected_unit.battle_id;

    record = &g_menu_unit_selection_records[unit_id];
    /* A nonzero second parameter makes the icon loop return here instead of exiting. */
    g_thread_contexts[g_thread_current_id].function_parameter_2 = 1;
    menu_run_icon_selection_loop();
    record->command_index = parameter->selected_index;
    thread_exit_current();
}

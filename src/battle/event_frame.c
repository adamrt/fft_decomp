/* Event-frame scheduling, tutorial input and pending script requests. */

#include "fft/ai.h"
#include "fft/battle_map.h"
#include "fft/battle_map_load.h"
#include "fft/camera.h"
#include "fft/cursor.h"
#include "fft/effect_script.h"
#include "fft/encounter.h"
#include "fft/equipment.h"
#include "fft/fade.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_window.h"
#include "fft/music.h"
#include "fft/overlay.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/status_screen.h"
#include "fft/system.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit_entd.h"
#include "fft/unit_event.h"
#include "fft/unit_list.h"
#include "fft/unit_panel.h"
#include "fft/weather.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

extern VECTOR* g_camera_published_coords_ptr;
extern SVECTOR* g_camera_published_rotation_ptr;
extern VECTOR g_camera_script_vector;

/* Reset to -1 at event entry. The request processor accepts a track ID here, but no named writer queues one. */
extern s16 g_sound_music_set_track_request;

/* The event frame compares slots 1..15 across its yield, including in WORLD. */
extern u8 g_event_thread_status_snapshot[THREAD_ID_COUNT];

/* Five rain and five snow commands; the dispatcher consumes each halfword's low byte.
 * WORLD retains this table even though it has no weather-request consumer. */
static u16 g_event_weather_commands[10] = { BATTLE_MAP_WEATHER_COMMAND_CLEAR, BATTLE_MAP_WEATHER_COMMAND_CLEAR,
    BATTLE_MAP_WEATHER_COMMAND_LIGHT_RAIN, BATTLE_MAP_WEATHER_COMMAND_RAIN, BATTLE_MAP_WEATHER_COMMAND_HEAVY_RAIN,
    BATTLE_MAP_WEATHER_COMMAND_CLEAR, BATTLE_MAP_WEATHER_COMMAND_CLEAR, BATTLE_MAP_WEATHER_COMMAND_LIGHT_SNOW,
    BATTLE_MAP_WEATHER_COMMAND_SNOW, BATTLE_MAP_WEATHER_COMMAND_HEAVY_SNOW };

static void camera_load_state_from_script_variables(void);
static void script_process_pending_requests(void);

/* While a tutorial scripts the input, hold wait_value in the tutorial wait
 * variable for one thread yield with every controller input disabled. */
void tutorial_pulse_wait_value(s32 wait_value) {
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, wait_value);
        script_set_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS, 0xFFFF);
        thread_yield();
        script_set_variable(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, 0);
    }
}

/* Apply disabled and forced input variables while retaining the high input bits. */
static void script_apply_input_overrides(u32* state) {
    u32 high;

    if (*state == INPUT_RESET_BUTTONS) {
        system_reset_game();
    }

    high = *state & 0xFFFF0000;
    *state = *state & (script_get_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) ^ 0xFFFF);
    *state = high | (script_get_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS) | *state);
}

/* Route controller input to the running battle event thread. */
void script_route_event_input(void) {
    s32 thread;

    if (g_menu_input_mode != MENU_INPUT_TUTORIAL) {
        return;
    }
    thread = g_thread_current_id;
    if (thread == THREAD_SLOT_MAIN) {
        return;
    }
    if (g_script_event_input == 0) {
        g_event_saved_input = 0;
    }
    if (g_thread_contexts[thread].task_id == THREAD_TASK_RESUME) {
        g_script_event_input = g_script_unfiltered_controller_input;
    } else {
        g_script_event_input = g_event_saved_input;
    }
}

/* Keep the raw 32-bit button value in g_script_unfiltered_controller_input, then
 * apply the script's disabled and forced inputs to it in place. */
void script_filter_controller_input(u32* input) {
    g_script_unfiltered_controller_input = *input;
    script_apply_input_overrides(input);
}

/* Run one event frame around the thread scheduler, returning a script_event_frame_result_e. */
s32 script_run_event_frame(u32* ot, u32 buttons) {
    RECT rect;
    s32 i;
    s32 finish_op;
    s32 next_event;
    u32 prims;
    u32* ordering_table;
    s32 polarity;

    g_ai_workspace_ptr = (event_work_t*)g_ai_workspace;
    /* While an END graphic is up (immediate drawing), reload its image from the high
     * overlay area into the frame buffer every frame. */
    if (g_gfx_draw_primitives_immediately != 0) {
        DrawSync(0);
        rect.w = 0x100;
        rect.x = 0;
        rect.h = 0xF0;
        rect.y = (g_gfx_screen_polarity & 1) * 0xF0;
        LoadImage(&rect, (u32*)g_heap_high_overlay_load_address);
    }
    g_gfx_current_otag_entry = ot;
    gfx_submit_screen_fade_overlay(&g_fade_overlays[g_menu_packet_buffer_index], g_gfx_screen_fade);
    camera_store_state_to_script_variables();
    menu_draw_active_window_frames();

    if (g_script_threads_idle != 0) {
        for (i = 1; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
            if (thread_is_running_asm(i) != 0) {
                break;
            }
        }
        if (i == VARIANT_THREAD_TABLE_SLOT_COUNT) {
            finish_op = scenario_get_finish_operation();
            if (finish_op != 0) {
                if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
                    g_script_threads_idle = 0;
                    script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                    return SCRIPT_EVENT_FRAME_TO_WORLD;
                }
            } else {
                g_script_threads_idle = 0;
                g_menu_overlay_state = 0;
                return SCRIPT_EVENT_FRAME_NO_FINISH;
            }
            if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_FUNERAL) {
                return SCRIPT_EVENT_FRAME_ENDING;
            }
            /* The finish word holds the next event id in bits 0-7 and 10-11 (its bits 8-9);
             * the remaining high bits give the finish result. */
            next_event = ((finish_op & 0xC00) >> 2) | (finish_op & 0xFF);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, next_event);
            finish_op = (finish_op & 0xF300) >> 8;
            g_script_threads_idle = 0;
            g_menu_overlay_state = 0;
            if (finish_op == SCRIPT_FINISH_TO_WORLD) {
                return SCRIPT_EVENT_FRAME_TO_WORLD;
            }
            if (finish_op == SCRIPT_FINISH_NEXT_EVENT) {
                return SCRIPT_EVENT_FRAME_NEXT_EVENT;
            }
            if (finish_op == SCRIPT_FINISH_RESET) {
                return SCRIPT_EVENT_FRAME_RESET;
            }
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, 0);
            g_encounter_selection_mode = UNIT_ENTD_SELECTION_MAP_ID;
            return SCRIPT_EVENT_FRAME_TO_WORLD;
        }
        g_script_threads_idle = 0;
    }

    script_update_event_input_state(buttons);
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        g_script_unfiltered_controller_input = g_script_event_input;
        script_apply_input_overrides(&g_script_event_input);
        g_event_saved_input = g_script_event_input;
        script_set_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);
    }
    if (g_event_map_reload_state >= 3) {
        g_event_map_reload_state = 0;
    }
    if (g_event_map_append_state >= 3) {
        g_event_map_append_state = 0;
        g_battle_map_destruction_wait_state = 1;
    }

    /* Clear last frame's requests; script commands raise them again while the threads run. */
    g_event_pending_map_jump_out_id = -1;
    g_event_weather_request = -1;
    g_event_pending_map_state = -1;
    g_event_pending_map_jump_out_2_id = -1;
    g_event_pending_effect_id = -1;
    g_script_pending_loaded_evtchr_slot = -1;
    g_event_pending_evtchr_save_slot = -1;
    g_event_pending_saved_evtchr_clear_slot = -1;
    g_event_pending_loaded_evtchr_clear_slot = -1;
    g_event_pending_unit_vram_copy = -1;
    g_event_pending_reserved_vram_release_slot = -1;
    g_sound_pending_sfx_id = -1;
    g_sound_music_volume_transition_request = -1;
    g_sound_music_set_track_request = -1;
    g_sound_music_switch_request = -1;
    g_sound_music_unload_slot_request = -1;
    g_event_map_jump_in_request = 0;
    g_event_3d_object_use_request = 0;
    g_event_field_object_use_request = 0;

    if (g_overlay_companion_overlay_state == COMPANION_OVERLAY_STATE_JOBSTTS) {
        ordering_table = jobstts_entrypoint(g_unit_panel_selected_unit.battle_id, 0);
        g_overlay_companion_overlay_primitives = (s32)ordering_table;
        if (ordering_table != 0) {
            AddPrims(g_gfx_current_otag_entry, ordering_table, &ordering_table[0x3f]);
        } else {
            g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        }
    }
    if (g_overlay_companion_overlay_state == COMPANION_OVERLAY_STATE_EQUIP) {
        prims = equip_entrypoint(g_unit_panel_selected_unit.battle_id, g_gfx_current_otag_entry + 1);
        g_overlay_companion_overlay_primitives = prims;
        if (prims >= 2) {
            AddPrims(g_gfx_current_otag_entry, (void*)prims, (void*)(prims + 0xFC));
        } else {
            g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        }
    }

    /* A press other than Select alone, or a thread starting or stopping while the threads
     * run (thread_yield), restarts the five-frame input suppression. */
    if (tutorial_is_current_event() == 0 && g_script_event_input != PSX_PAD_SELECT && g_script_event_input != 0) {
        g_script_input_suppression_frames = 5;
    }
    for (i = 1; i < THREAD_ID_COUNT; i++) {
        g_event_thread_status_snapshot[i] = thread_is_running_asm(i);
    }
    thread_yield();
    for (i = 1; i < THREAD_ID_COUNT; i++) {
        if (g_event_thread_status_snapshot[i] != thread_is_running_asm(i)) {
            g_script_input_suppression_frames = 5;
        }
    }
    if (g_script_input_suppression_frames != 0) {
        g_script_input_suppression_frames--;
    }

    camera_load_state_from_script_variables();
    g_event_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_menu_input_block_frames != 0) {
        g_menu_input_block_frames--;
    }
    script_frame_after_threads_stub();
    if (g_script_frame_result_override != 0) {
        return g_script_frame_result_override;
    }

    if (g_overlay_companion_overlay_state == COMPANION_OVERLAY_STATE_STUB) {
        overlay_companion_stub();
        g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
    } else if (g_overlay_companion_overlay_state == COMPANION_OVERLAY_STATE_BUNIT) {
        script_set_event_speed(1);
        i = bunit_entrypoint(0, 0);
        cursor_move_to_battle_id(i);
        g_unit_panel_selected_unit.battle_id = i;
        g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
    } else if (g_overlay_companion_overlay_state == COMPANION_OVERLAY_STATE_CARD_SAVE) {
        script_set_event_speed(1);
        /* The save menu runs frames of its own and changes the screen polarity; keep the event's. */
        polarity = g_gfx_screen_polarity;
        script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
        card_entrypoint(0);
        g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        g_gfx_screen_polarity = polarity;
    }

    overlay_poll_companion();
    overlay_poll_companion_file();
    script_process_pending_requests();
    script_frame_after_requests_stub();
    if (g_menu_input_mode == MENU_INPUT_BLOCKED) {
        return SCRIPT_EVENT_FRAME_RUNNING;
    }
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        return SCRIPT_EVENT_FRAME_NO_FINISH;
    }

    /* Once every thread has stopped, the next frame reports the scenario finish. */
    for (i = 1; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
        if (thread_is_running_asm(i) != 0) {
            break;
        }
    }
    if (i == VARIANT_THREAD_TABLE_SLOT_COUNT) {
        g_script_threads_idle = 1;
    }
    return SCRIPT_EVENT_FRAME_RUNNING;
}

/* Feed `input` to the event input state, make `entry` the current ordering-table entry
 * and draw the active menu windows. No retail code calls it. */
void script_set_current_otag_entry_and_update_input(u32* entry, s32 input) {
    script_update_event_input_state(input);
    g_gfx_current_otag_entry = entry;
    menu_draw_active_window_frames();
}

/* Advance the map-destruction wait state from 1 (armed when a map append
 * finishes) to 2. script_process_pending_requests then releases the held
 * GNS data and polls the map load, returning the state to 0 when the load ends.
 * Any other state is left alone. */
void script_set_specialized_map_destroyed(void) {
    if (g_battle_map_destruction_wait_state == 1) {
        g_battle_map_destruction_wait_state = g_battle_map_destruction_wait_state + 1;
    }
}

/* Advance asynchronous battle-script, map, effect, graphics and sound requests. */
static void script_process_pending_requests(void) {
    s16 destruction_state;
    s16 effect_id;
    s16 jump_out_map_id;
    s16 jump_out_2_map_id;
    s16 jump_in_duration;
    s16 time_of_day;
    s16 weather_request;
    s32 weather_id;
    s32 loaded_slot;
    s32 save_slot;
    s32 saved_clear_slot;
    s32 loaded_clear_slot;
    s32 release_slot;
    s32 sfx_id;
    s32 unit_vram_copy;
    u16 reload_state;
    s32 append_state;
    u16 music_track_id;
    u32 volume_transition;
    s32 pending = 0;
    effect_start_target_data_t effect_target;

    unit_update_facing_and_pending_animations();

    jump_out_map_id = g_event_pending_map_jump_out_id;
    if (jump_out_map_id != -1) {
        state_start_map_jump_out(jump_out_map_id, 0x10);
    }
    jump_out_2_map_id = g_event_pending_map_jump_out_2_id;
    if (jump_out_2_map_id != -1) {
        state_start_change_map_jump_out(jump_out_2_map_id, 0x10);
    }
    jump_in_duration = g_event_map_jump_in_request;
    if (jump_in_duration != 0) {
        state_start_change_map_jump_in(jump_in_duration);
    }

    if (g_event_3d_object_use_request != 0) {
        battle_map_set_3d_object_state(g_event_3d_object_id, g_event_3d_object_state);
        g_3d_object_wait_status = 1;
    }
    if (g_event_field_object_use_request != 0) {
        battle_map_texture_start_animation(g_event_map_command_83_arg1, g_event_map_command_83_arg2);
        g_field_object_wait_status = 1;
    }
    if (g_3d_object_wait_status != 0) {
        g_3d_object_wait_status = battle_map_get_3d_object_state(g_event_3d_object_id);
    }
    if (g_field_object_wait_status != 0) {
        g_field_object_wait_status = battle_map_texture_get_animation_active(g_event_map_command_83_arg1);
    }

    if (g_battle_map_refresh_pending != 0) {
        g_battle_map_refresh_pending = battle_map_load_alternate_mesh(3);
    }
    /* Map reload and append step through states: 1 starts the load, 2 polls it, and 3 (done)
     * is cleared at the start of the next frame. */
    reload_state = g_event_map_reload_state;
    if (reload_state == 1) {
        if (battle_map_load_start() == 0) {
            g_event_map_reload_state = g_event_map_reload_state + 1;
        }
    } else if (reload_state == 2) {
        if (battle_map_load_poll() == 0) {
            g_event_map_reload_state = g_event_map_reload_state + 1;
        }
    }
    append_state = g_event_map_append_state;
    if ((append_state == 1) && (g_battle_map_destruction_wait_state == 0)) {
        battle_map_load_data(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), BATTLE_MAP_LOAD_COMMAND_START_HELD);
        g_event_map_append_state = g_event_map_append_state + 1;
    } else if ((append_state == 2)
        && (battle_map_load_data(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), BATTLE_MAP_LOAD_COMMAND_POLL_HELD)
            == BATTLE_MAP_LOAD_RESULT_HELD)) {
        g_event_map_append_state = g_event_map_append_state + 1;
    }
    /* State 2 (set once a script or effect reports the map destroyed) releases the held map data;
     * 3 polls the load until it ends. */
    destruction_state = g_battle_map_destruction_wait_state;
    if (destruction_state == 2) {
        g_battle_map_destruction_wait_state = destruction_state + 1;
        battle_map_dispatch_data_command(BATTLE_MAP_DATA_COMMAND_RELEASE_GNS_HOLD, 1, 1, 1);
    } else if ((destruction_state == 3)
        && (battle_map_load_data(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), BATTLE_MAP_LOAD_COMMAND_POLL)
            == 0)) {
        g_battle_map_destruction_wait_state = 0;
    }

    /* The low nibble picks the weather; without 0xF00 bits the request only clears. Snowy
     * maps use the snow variants five entries later. */
    weather_request = g_event_weather_request;
    if (weather_request != -1) {
        weather_id = weather_request & 0xF;
        if (!(weather_request & 0xF00)) {
            g_event_weather_request = -1;
        } else {
            g_event_weather_request = weather_id;
            if (g_weather_flags & BATTLE_MAP_WEATHER_FLAG_SNOW) {
                g_event_weather_request = weather_id + 5;
            }
            weather_run_command(g_event_weather_commands[g_event_weather_request]);
        }
    }

    time_of_day = g_event_pending_map_state;
    if (time_of_day != -1) {
        script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, time_of_day);
        weather_run_command(BATTLE_MAP_WEATHER_COMMAND_APPLY_SCRIPT_WEATHER);
    }

    effect_id = g_event_pending_effect_id;
    if (effect_id != -1) {
        if (g_event_effect_target_mode == EFFECT_START_ON_CASTER) {
            effect_target.runtime_id = g_event_effect_target_runtime_id;
            effect_start_new(effect_id, EFFECT_START_ON_CASTER, effect_target);
        } else {
            effect_target.runtime_id = g_event_effect_target_runtime_id;
            effect_start_new(effect_id, EFFECT_START_ON_UNIT, effect_target);
        }
    }

    if (g_unit_graphics_load_pending != 0) {
        g_unit_graphics_load_pending = sprite_poll_unit_graphics_load();
    }

    /* `pending` is never set, so the slot and sound requests always run. */
    if (pending == 0) {
        loaded_slot = g_script_pending_loaded_evtchr_slot;
        if (loaded_slot >= 0) {
            sprite_evtchr_unpack_to_slot(loaded_slot, g_script_loaded_evtchr_buffer);
        }
        save_slot = g_event_pending_evtchr_save_slot;
        if (save_slot >= 0) {
            sprite_evtchr_save_slot(save_slot);
        }
        saved_clear_slot = g_event_pending_saved_evtchr_clear_slot;
        if (saved_clear_slot >= 0) {
            sprite_evtchr_clear_saved_slot(saved_clear_slot);
        }
        loaded_clear_slot = g_event_pending_loaded_evtchr_clear_slot;
        if (loaded_clear_slot >= 0) {
            sprite_evtchr_clear_loaded_slot(loaded_clear_slot);
        }
        /* High byte: the unit's runtime id; low byte: the event-character slot. */
        unit_vram_copy = g_event_pending_unit_vram_copy;
        if (unit_vram_copy >= 0) {
            sprite_evtchr_load_unit_attack_image((unit_vram_copy & 0xFF00) >> 8, unit_vram_copy & 0xFF);
        }
        release_slot = g_event_pending_reserved_vram_release_slot;
        if (release_slot >= 0) {
            sprite_evtchr_release_reserved_slot(release_slot);
        }

        sfx_id = g_sound_pending_sfx_id;
        if (sfx_id != -1) {
            sound_play_sfx(sfx_id);
        }
        /* Alternate between the two track slots; nothing new starts when that slot is empty. */
        if (g_sound_music_switch_request >= 0) {
            music_stop();
            g_sound_current_music_track_index = g_sound_current_music_track_index ^ 1;
            if (g_sound_current_music_track_index == 0) {
                music_track_id = g_sound_music_track_1_id;
            } else {
                music_track_id = g_sound_music_track_2_id;
            }
            if (music_track_id != 0) {
                music_switch_track(g_sound_current_music_track_index + 1,
                    script_scale_music_volume(g_event_music_switch_volume), g_event_music_switch_fade * 4);
            }
        }
        if (g_sound_music_unload_slot_request >= 0) {
            music_stop();
            music_unload_scenario_slot(g_sound_music_unload_slot_request);
        }
        if (g_sound_music_set_track_request != -1) {
            music_unload_current_scenario();
            music_set_track(g_sound_music_set_track_request);
        }
        /* Low half: target volume; high half: fade time, passed times four like the switch fade. */
        volume_transition = g_sound_music_volume_transition_request;
        if (volume_transition != -1U) {
            music_set_volume_target(volume_transition & 0xFFFF, (volume_transition >> 0xE) & 0x3FFC);
        }
    }
}

/* Load camera rotation, zoom and position back out of the script variables. */
static void camera_load_state_from_script_variables(void) {
    setVector(&g_camera_script_rotation, g_script_variables[EVENT_SCRIPT_VAR_CAMERA_TILT],
        g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW], g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ROLL]);
    camera_rotation_set(&g_camera_script_rotation);

    setVector(&g_camera_script_vector, g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM],
        g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM], g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM]);
    camera_zoom_set(&g_camera_script_vector);

    g_camera_script_vector.vx = g_script_variables[EVENT_SCRIPT_VAR_CAMERA_X];
    g_camera_script_vector.vy = (g_camera_shake_z_offset * ONE) + g_script_variables[EVENT_SCRIPT_VAR_CAMERA_Z];
    g_camera_script_vector.vz = g_script_variables[EVENT_SCRIPT_VAR_CAMERA_Y];
    camera_real_coords_set(&g_camera_script_vector);
}

/* Publish the live camera rotation, zoom and position to the script variables.
 * The three zoom components all land in the same 32-bit variable. */
void camera_store_state_to_script_variables(void) {
    SVECTOR* rotation;
    VECTOR* zoom;
    VECTOR* coords;

    rotation = camera_rotation_get();
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_TILT] = rotation->vx;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW] = rotation->vy;
    g_camera_published_rotation_ptr = rotation;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ROLL] = rotation->vz;

    zoom = camera_zoom_get();
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM] = zoom->vx;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM] = zoom->vy;
    g_camera_published_coords_ptr = zoom;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_ZOOM] = zoom->vz;

    coords = camera_real_coords_get();
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_X] = coords->vx;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_Z] = coords->vy - (g_camera_shake_z_offset * ONE);
    g_camera_published_coords_ptr = coords;
    g_camera_shake_z_offset = 0;
    g_script_variables[EVENT_SCRIPT_VAR_CAMERA_Y] = coords->vz;
}

/* The raw weather script variable. weather_get_effective applies the
 * map's indoor and snow flags to it. */
s32 battle_map_get_weather(void) {
    return g_script_variables[EVENT_SCRIPT_VAR_WEATHER];
}

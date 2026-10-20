/* Event-frame scheduling, tutorial input and pending script requests. */

#include "fft/ai.h"
#include "fft/battle_map.h"
#include "fft/camera.h"
#include "fft/cursor.h"
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
#include "fft/overlay.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/status_screen.h"
#include "fft/system.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit_entd.h"
#include "fft/unit_list.h"
#include "fft/unit_panel.h"
#include "fft/weather.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* event input state */
extern u32 g_script_current_input;
extern u32 g_script_previous_input;
extern u32 g_script_initial_repeat_counter;
extern u32 g_script_repeat_counter;
extern u32 g_script_secondary_repeat_counter;

/* The event frame compares slots 1..15 across its yield, including in WORLD. */
extern u8 g_event_thread_status_snapshot[THREAD_ID_COUNT];

/* Five rain and five snow commands; the dispatcher consumes each halfword's low byte.
 * WORLD retains this table even though it has no weather-request consumer. */
static u16 g_event_weather_commands[10] = { BATTLE_MAP_WEATHER_COMMAND_CLEAR, BATTLE_MAP_WEATHER_COMMAND_CLEAR,
    BATTLE_MAP_WEATHER_COMMAND_LIGHT_RAIN, BATTLE_MAP_WEATHER_COMMAND_RAIN, BATTLE_MAP_WEATHER_COMMAND_HEAVY_RAIN,
    BATTLE_MAP_WEATHER_COMMAND_CLEAR, BATTLE_MAP_WEATHER_COMMAND_CLEAR, BATTLE_MAP_WEATHER_COMMAND_LIGHT_SNOW,
    BATTLE_MAP_WEATHER_COMMAND_SNOW, BATTLE_MAP_WEATHER_COMMAND_HEAVY_SNOW };

static void camera_from_script_variables_load(void);
static void pending_requests_process(void);

/* While a tutorial scripts the input, hold wait_value in the tutorial wait
 * variable for one thread yield with every controller input disabled. */
void tutorial_pulse_wait_value(s32 wait_value) {
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        script_variable_set(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, wait_value);
        script_variable_set(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS, 0xFFFF);
        thread_yield();
        script_variable_set(EVENT_SCRIPT_VAR_TUTORIAL_WAIT_VALUE, 0);
    }
}

/* Apply disabled and forced input variables while retaining the high input bits. */
static void input_overrides_apply(u32* state) {
    u32 high;

    if (*state == INPUT_RESET_BUTTONS) {
        system_game_reset();
    }

    high = *state & 0xFFFF0000;
    *state = *state & (script_variable_get(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) ^ 0xFFFF);
    *state = high | (script_variable_get(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS) | *state);
}

/* While the tutorial text thread is current, latch its input and apply the
 * initial, repeat and secondary repeat delays. */
void script_route_event_input(void) {
    u32 input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_repeat_delay;
    u32 previous_input;
    u32 counter;

    if (g_thread_current_id != SCREEN_THREAD_SLOT_TUTORIAL_TEXT) {
        return;
    }
    input = tutorial_buttons_get();
    if (input == 0) {
        g_script_initial_repeat_counter = 0;
        g_script_repeat_counter = 0;
        g_script_secondary_repeat_counter = 0;
    }

    previous_input = g_script_current_input;
    g_script_current_input = input;
    g_script_event_current_input = input;
    g_script_event_input = 0;
    g_script_previous_input = previous_input;

    if (previous_input != input || (input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK)) == 0) {
        g_script_event_input = ~previous_input & input;
        g_script_initial_repeat_counter = 0;
    } else {
        initial_delay = g_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_script_initial_repeat_counter;
        if (counter == initial_delay) {
            g_script_event_input = input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK);
        }
        if (counter >= initial_delay) {
            repeat_delay = g_input_repeat_period / g_script_event_speed;
            counter = ++g_script_repeat_counter;
            g_script_secondary_repeat_counter++;
            if (counter >= repeat_delay) {
                g_script_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_script_event_input = input & PSX_PAD_DPAD_MASK;
                }
            }
            secondary_repeat_delay = g_input_secondary_repeat_period / g_script_event_speed;
            if (g_script_secondary_repeat_counter >= secondary_repeat_delay) {
                g_script_secondary_repeat_counter = 0;
                if ((g_script_current_input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input
                        |= g_script_current_input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_DOWN);
                } else {
                    g_script_event_input |= g_script_current_input & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_script_repeat_counter = 0;
            g_script_secondary_repeat_counter = 0;
        }
    }
}

/* Keep the raw 32-bit button value in g_script_unfiltered_controller_input, then
 * apply the script's disabled and forced inputs to it in place. */
void script_filter_controller_input(u32* input) {
    g_script_unfiltered_controller_input = *input;
    input_overrides_apply(input);
}

/* Run one event frame around the thread scheduler, returning a script_event_frame_result_e. */
s32 script_event_frame_run(u32* ot, u32 buttons) {
    RECT rect;
    s32 i;
    s32 finish_op;
    s32 next_event;
    u32 prims;
    u32* ordering_table;
    s32 polarity;

    g_event_work = (event_work_t*)g_ai_workspace;
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
    camera_state_to_script_variables_store();
    menu_draw_active_window_frames();

    if (g_script_threads_idle != 0) {
        for (i = 1; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
            if (thread_is_running_asm(i) != 0) {
                break;
            }
        }
        if (i == VARIANT_THREAD_TABLE_SLOT_COUNT) {
            finish_op = scenario_finish_operation_get();
            if (finish_op != 0) {
                if (script_variable_get(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
                    g_script_threads_idle = 0;
                    script_variable_set(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                    return SCRIPT_EVENT_FRAME_TO_WORLD;
                }
            } else {
                g_script_threads_idle = 0;
                g_menu_overlay_state = 0;
                return SCRIPT_EVENT_FRAME_NO_FINISH;
            }
            if (script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_FUNERAL) {
                return SCRIPT_EVENT_FRAME_ENDING;
            }
            /* The finish word holds the next event id in bits 0-7 and 10-11 (its bits 8-9);
             * the remaining high bits give the finish result. */
            next_event = ((finish_op & 0xC00) >> 2) | (finish_op & 0xFF);
            script_variable_set(EVENT_SCRIPT_VAR_CURRENT_EVENT, next_event);
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
            script_variable_set(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
            script_variable_set(EVENT_SCRIPT_VAR_CURRENT_ENTD, 0);
            g_encounter_selection_mode = UNIT_ENTD_SELECTION_MAP_ID;
            return SCRIPT_EVENT_FRAME_TO_WORLD;
        }
        g_script_threads_idle = 0;
    }

    script_event_input_update(buttons);
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        g_script_unfiltered_controller_input = g_script_event_input;
        input_overrides_apply(&g_script_event_input);
        g_event_saved_input = g_script_event_input;
        script_variable_set(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);
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

    script_load_event_input_state((menu_input_state_t*)&g_script_event_input);
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

    camera_from_script_variables_load();
    g_frame_counter++;
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
        script_event_speed_set(1);
        i = bunit_entrypoint(0, 0);
        cursor_unit_tile_move(i);
        g_unit_panel_selected_unit.battle_id = i;
        g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
    } else if (g_overlay_companion_overlay_state == COMPANION_OVERLAY_STATE_CARD_SAVE) {
        script_event_speed_set(1);
        /* The save menu runs frames of its own and changes the screen polarity; keep the event's. */
        polarity = g_gfx_screen_polarity;
        script_variable_set(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
        card_entrypoint(0);
        g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        g_gfx_screen_polarity = polarity;
    }

    overlay_companion_poll();
    overlay_companion_file_poll();
    pending_requests_process();
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
    script_event_input_update(input);
    g_gfx_current_otag_entry = entry;
    menu_draw_active_window_frames();
}

/* Advance the retained map-destruction wait state from 1 to 2.
 * This program's empty request hook does not advance it further. */
void script_map_destruction_signal(void) {
    if (g_battle_map_destruction_wait_state == 1) {
        g_battle_map_destruction_wait_state = g_battle_map_destruction_wait_state + 1;
    }
}

/* This program retains the event request hook as an empty function. */
static void pending_requests_process(void) {
}

/* This program retains the camera-load hook as an empty function. */
static void camera_from_script_variables_load(void) {
}

/* This program retains the camera-store hook as an empty function. */
void camera_state_to_script_variables_store(void) {
}

/* The raw weather script variable. weather_get_effective applies the
 * map's indoor and snow flags to it. */
s32 battle_map_weather_get(void) {
    return g_script_variables[EVENT_SCRIPT_VAR_WEATHER];
}

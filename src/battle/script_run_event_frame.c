#include "fft/battle_action.h"
#include "fft/battle_event.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/event_bunit.h"
#include "fft/event_card.h"
#include "fft/event_equip.h"
#include "fft/event_jobstts.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Run one frame of BATTLE bookkeeping around the thread scheduler.
 *
 * BATTLE twin of script_run_event_frame: returns the scenario-finish result
 * (9, 0xA, 0xB or 0x13, 0 when no finish operation is pending) once every
 * script thread has stopped, otherwise 1 (or g_script_frame_result_override while it is set, or 0
 * while menu input is disabled with state 2). The three BATTLE-only 16-bit clears come
 * after the 32-bit clears in source; the scheduler still groups them with the
 * other 16-bit stores, and this order keeps the g_battle_companion_overlay_state load
 * below them as in the target.
 */
s32 script_run_event_frame(u32* ot, u32 buttons) {
    RECT rect;
    s32 i;
    s32 finish_op;
    u32 prims;
    u32* ordering_table;
    s32 polarity;

    g_ai_workspace_ptr = (event_work_t*)g_ai_workspace;
    if (g_gfx_draw_primitives_immediately != 0) {
        DrawSync(0);
        rect.w = 0x100;
        rect.x = 0;
        rect.h = 0xF0;
        rect.y = (g_main_gfx_screen_polarity & 1) * 0xF0;
        LoadImage(&rect, (u32*)g_main_heap_high_overlay_load_address);
    }
    g_gfx_current_otag_entry = ot;
    gfx_submit_screen_fade_overlay(&g_battle_gfx_screen_fade_overlays[g_menu_packet_buffer_index], g_gfx_screen_fade);
    camera_store_state_to_script_variables();
    menu_draw_active_window_frames();

    if (g_script_threads_idle != 0) {
        for (i = 1; i < 16; i++) {
            if (battle_thread_is_running_asm(i) != 0) {
                break;
            }
        }
        if (i == 16) {
            finish_op = script_get_current_scenario_finish_operation();
            if (finish_op != 0) {
                if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
                    g_script_threads_idle = 0;
                    script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                    return 9;
                }
            } else {
                g_script_threads_idle = 0;
                g_menu_overlay_state = 0;
                return 0;
            }
            if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_FUNERAL) {
                return 0x13;
            }
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, ((finish_op & 0xC00) >> 2) | (finish_op & 0xFF));
            finish_op = (finish_op & 0xF300) >> 8;
            g_script_threads_idle = 0;
            g_menu_overlay_state = 0;
            if (finish_op == 0x80) {
                return 9;
            }
            if (finish_op == 0x81) {
                return 0xA;
            }
            if (finish_op == 0x82) {
                return 0xB;
            }
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, 0);
            g_battle_entd_selection_mode = BATTLE_ENTD_SELECTION_MAP_ID;
            return 9;
        }
        g_script_threads_idle = 0;
    }

    script_update_event_input_state(buttons);
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        g_script_unfiltered_controller_input = g_script_event_input;
        script_apply_input_overrides(&g_script_event_input);
        g_script_saved_event_input = g_script_event_input;
        script_set_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);
    }
    if (g_map_reload_state >= 3) {
        g_map_reload_state = 0;
    }
    if (g_map_append_state >= 3) {
        g_map_append_state = 0;
        g_map_destruction_wait_state = 1;
    }
    g_battle_event_pending_map_jump_out_id = -1;
    g_battle_event_weather_request = -1;
    g_battle_event_pending_map_state = -1;
    g_battle_event_pending_map_jump_out_2_id = -1;
    g_battle_event_pending_effect_id = -1;
    g_script_pending_loaded_evtchr_slot = -1;
    g_battle_event_pending_evtchr_save_slot = -1;
    g_battle_event_pending_saved_evtchr_clear_slot = -1;
    g_battle_event_pending_loaded_evtchr_clear_slot = -1;
    g_battle_event_pending_unit_vram_copy = -1;
    g_battle_event_pending_reserved_vram_release_slot = -1;
    g_sound_pending_sfx_id = -1;
    g_sound_music_volume_transition_request = -1;
    g_sound_music_set_track_request = -1;
    g_sound_music_switch_request = -1;
    g_sound_music_unload_slot_request = -1;
    g_battle_event_map_jump_in_request = 0;
    g_battle_3d_object_use_request = 0;
    g_battle_field_object_use_request = 0;

    if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_JOBSTTS) {
        ordering_table = jobstts_entrypoint(g_menu_selected_unit.battle_id, 0);
        g_battle_companion_overlay_primitives = (s32)ordering_table;
        if (ordering_table != 0) {
            AddPrims(g_gfx_current_otag_entry, ordering_table, &ordering_table[0x3f]);
        } else {
            g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        }
    }
    if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_EQUIP) {
        prims = equip_entrypoint(g_menu_selected_unit.battle_id, g_gfx_current_otag_entry + 1);
        g_battle_companion_overlay_primitives = prims;
        if (prims >= 2) {
            AddPrims(g_gfx_current_otag_entry, (void*)prims, (void*)(prims + 0xFC));
        } else {
            g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        }
    }

    if (script_is_tutorial_event_slot() == 0 && g_script_event_input != PSX_PAD_SELECT && g_script_event_input != 0) {
        g_script_input_suppression_frames = 5;
    }
    for (i = 1; i < 16; i++) {
        g_battle_event_thread_status_snapshot[i] = battle_thread_is_running_asm(i);
    }
    thread_yield();
    for (i = 1; i < 16; i++) {
        if (g_battle_event_thread_status_snapshot[i] != battle_thread_is_running_asm(i)) {
            g_script_input_suppression_frames = 5;
        }
    }
    if (g_script_input_suppression_frames != 0) {
        g_script_input_suppression_frames--;
    }
    camera_load_state_from_script_variables();
    g_battle_event_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_battle_menu_input_block_frames != 0) {
        g_battle_menu_input_block_frames--;
    }
    script_frame_after_threads_stub();
    if (g_script_frame_result_override != 0) {
        return g_script_frame_result_override;
    }

    if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_STUB) {
        menu_run_companion_overlay_stub();
        g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
    } else if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_BUNIT) {
        script_set_event_speed(1);
        i = bunit_entrypoint(0, 0);
        target_move_cursor_to_battle_id(i);
        g_menu_selected_unit.battle_id = i;
        g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
    } else if (g_menu_companion_overlay_state == COMPANION_OVERLAY_STATE_CARD_SAVE) {
        script_set_event_speed(1);
        polarity = g_main_gfx_screen_polarity;
        script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
        card_save_run_menu(0);
        g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        g_main_gfx_screen_polarity = polarity;
    }
    menu_open_companion_executable();
    menu_open_companion_data_file();
    script_process_pending_requests();
    script_frame_after_requests_stub();
    if (g_menu_input_mode == MENU_INPUT_BLOCKED) {
        return 1;
    }
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        return 0;
    }
    for (i = 1; i < 16; i++) {
        if (battle_thread_is_running_asm(i) != 0) {
            break;
        }
    }
    if (i == 16) {
        g_script_threads_idle = 1;
    }
    return 1;
}

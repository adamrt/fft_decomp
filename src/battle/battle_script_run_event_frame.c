#include "fft/battle.h"
#include "fft/event_bunit.h"
#include "fft/event_card.h"
#include "fft/event_equip.h"
#include "fft/event_jobstts.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Run one frame of BATTLE bookkeeping around the thread scheduler.
 *
 * BATTLE twin of world_script_run_frame: returns the scenario-finish result
 * (9, 0xA, 0xB or 0x13, 0 when no finish operation is pending) once every
 * script thread has stopped, otherwise 1 (or g_battle_script_frame_result_override while it is set, or 0
 * while menu input is disabled with state 2). The three BATTLE-only halfword clears come
 * after the word clears in source; the scheduler still groups them with the
 * other halfword stores, and this order keeps the g_companion_overlay_state load
 * below them as in the target.
 */
s32 battle_script_run_event_frame(u32* ot, u32 buttons) {
    RECT rect;
    s32 i;
    s32 finish_op;
    u32 prims;
    s32 polarity;

    g_battle_ai_workspace_ptr = (void*)g_battle_ai_workspace;
    if (g_battle_etc_graphics_enabled != 0) {
        DrawSync(0);
        rect.w = 0x100;
        rect.x = 0;
        rect.h = 0xF0;
        rect.y = (g_main_gfx_screen_polarity & 1) * 0xF0;
        LoadImage(&rect, (u32*)g_main_heap_high_overlay_load_address);
    }
    g_current_otag_entry = ot;
    battle_gfx_submit_screen_fade_overlay(
        &g_battle_gfx_screen_fade_overlays[g_battle_menu_packet_buffer_index], g_battle_screen_fade);
    battle_camera_store_state_to_script_variables();
    battle_menu_draw_active_window_frames();

    if (g_battle_script_threads_idle != 0) {
        for (i = 1; i < 16; i++) {
            if (battle_thread_is_running_8014cc94(i) != 0) {
                break;
            }
        }
        if (i == 16) {
            finish_op = battle_script_get_current_scenario_finish_operation();
            if (finish_op != 0) {
                if (battle_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
                    g_battle_script_threads_idle = 0;
                    battle_script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                    return 9;
                }
            } else {
                g_battle_script_threads_idle = 0;
                g_option_menu_submenu_state = 0;
                return 0;
            }
            if (battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == 0x147) {
                return 0x13;
            }
            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, ((finish_op & 0xC00) >> 2) | (finish_op & 0xFF));
            finish_op = (finish_op & 0xF300) >> 8;
            g_battle_script_threads_idle = 0;
            g_option_menu_submenu_state = 0;
            if (finish_op == 0x80) {
                return 9;
            }
            if (finish_op == 0x81) {
                return 0xA;
            }
            if (finish_op == 0x82) {
                return 0xB;
            }
            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
            battle_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, 0);
            g_battle_entd_selection_mode = 4;
            return 9;
        }
        g_battle_script_threads_idle = 0;
    }

    battle_script_update_event_input_state(buttons);
    if (g_battle_menu_input_disabled == 2) {
        g_battle_script_unfiltered_controller_input = g_battle_script_event_input;
        battle_script_apply_input_overrides((u32*)&g_battle_script_event_input);
        g_battle_script_saved_event_input = g_battle_script_event_input;
        battle_script_set_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);
    }
    if (g_battle_map_reload_state >= 3) {
        g_battle_map_reload_state = 0;
    }
    if (g_battle_map_append_state >= 3) {
        g_battle_map_append_state = 0;
        g_battle_map_destruction_wait_state = 1;
    }
    g_battle_event_pending_map_jump_out_id = -1;
    g_battle_event_weather_request = -1;
    g_battle_event_pending_map_state = -1;
    g_battle_event_pending_map_jump_out_2_id = -1;
    g_battle_event_pending_effect_id = -1;
    g_battle_event_pending_loaded_evtchr_slot = -1;
    g_battle_event_pending_evtchr_save_slot = -1;
    g_battle_event_pending_saved_evtchr_clear_slot = -1;
    g_battle_event_pending_loaded_evtchr_clear_slot = -1;
    g_battle_event_pending_unit_vram_copy = -1;
    g_battle_event_pending_reserved_vram_release_slot = -1;
    g_sound_effect_id_to_play = -1;
    g_battle_music_volume_transition_request = -1;
    g_battle_music_set_track_request = -1;
    g_battle_music_switch_request = -1;
    g_battle_music_unload_slot_request = -1;
    g_battle_event_map_jump_in_request = 0;
    g_battle_3d_object_use_request = 0;
    g_battle_field_object_use_request = 0;

    if (g_companion_overlay_state == 3) {
        /* The target also passes a1 = 0 to the one-argument entry point. */
        g_battle_companion_overlay_primitives
            = ((s32 (*)(s32, s32))jobstts_entrypoint)(g_battle_active_turn_unit.battle_id, 0);
        if (g_battle_companion_overlay_primitives != 0) {
            AddPrims(g_current_otag_entry, (void*)g_battle_companion_overlay_primitives,
                (void*)(g_battle_companion_overlay_primitives + 0xFC));
        } else {
            g_companion_overlay_state = 0;
        }
    }
    if (g_companion_overlay_state == 4) {
        prims = equip_entrypoint(g_battle_active_turn_unit.battle_id, g_current_otag_entry + 1);
        g_battle_companion_overlay_primitives = prims;
        if (prims >= 2) {
            AddPrims(g_current_otag_entry, (void*)prims, (void*)(prims + 0xFC));
        } else {
            g_companion_overlay_state = 0;
        }
    }

    if (battle_script_is_tutorial_event_slot() == 0 && g_battle_script_event_input != PSX_PAD_SELECT
        && g_battle_script_event_input != 0) {
        g_event_input_suppression_frames = 5;
    }
    for (i = 1; i < 16; i++) {
        g_battle_event_thread_status_snapshot[i] = battle_thread_is_running_8014cc94(i);
    }
    battle_thread_yield();
    for (i = 1; i < 16; i++) {
        if (g_battle_event_thread_status_snapshot[i] != battle_thread_is_running_8014cc94(i)) {
            g_event_input_suppression_frames = 5;
        }
    }
    if (g_event_input_suppression_frames != 0) {
        g_event_input_suppression_frames--;
    }
    battle_camera_load_state_from_script_variables();
    g_battle_event_frame_counter++;
    g_battle_menu_packet_buffer_index = (g_battle_menu_packet_buffer_index + 1) & 1;
    if (g_battle_menu_input_block_frames != 0) {
        g_battle_menu_input_block_frames--;
    }
    battle_noop_8013da70();
    if (g_battle_script_frame_result_override != 0) {
        return g_battle_script_frame_result_override;
    }

    if (g_companion_overlay_state == 1) {
        battle_noop_8013ccec();
        g_companion_overlay_state = 0;
    } else if (g_companion_overlay_state == 2) {
        battle_script_set_event_speed(1);
        /* The target passes a0 = a1 = 0 to the argument-less entry point. */
        i = ((s32 (*)(s32, s32))bunit_entrypoint)(0, 0);
        battle_target_move_cursor_to_battle_id(i);
        g_battle_active_turn_unit.battle_id = i;
        g_companion_overlay_state = 0;
    } else if (g_companion_overlay_state == 5) {
        battle_script_set_event_speed(1);
        polarity = g_main_gfx_screen_polarity;
        battle_script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
        card_save_run_menu(0);
        g_companion_overlay_state = 0;
        g_main_gfx_screen_polarity = polarity;
    }
    battle_menu_open_companion_executable();
    battle_menu_open_secondary_companion_executable();
    battle_script_process_pending_requests();
    battle_noop_8014a82c();
    if (g_battle_menu_input_disabled == 1) {
        return 1;
    }
    if (g_battle_menu_input_disabled == 2) {
        return 0;
    }
    for (i = 1; i < 16; i++) {
        if (battle_thread_is_running_8014cc94(i) != 0) {
            break;
        }
    }
    if (i == 16) {
        g_battle_script_threads_idle = 1;
    }
    return 1;
}

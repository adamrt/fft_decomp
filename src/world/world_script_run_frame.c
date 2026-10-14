#include "fft/battle_target.h"
#include "fft/event_bunit.h"
#include "fft/event_card.h"
#include "fft/event_equip.h"
#include "fft/event_jobstts.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Run one frame of WORLD bookkeeping around the thread scheduler.
 *
 * Returns the scenario-finish result (9, 0xA, 0xB or 0x13, 0 when no finish
 * operation is pending) once every script thread has stopped, otherwise 1
 * (or g_script_frame_result_override while it is set, or 0 while menu input is disabled with state
 * 2). The companion-overlay entry points at 0x801BF3B8-0x801DF198 are overlay
 * addresses whose owner depends on what is loaded, so they keep func_ names.
 * The finish-operation getter is called through an s32-returning cast: the
 * target uses its return value without the u16 zero extension.
 */
s32 world_script_run_frame(u32* ot, u32 buttons) {
    RECT rect;
    s32 i;
    s32 op;
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
    world_gfx_submit_screen_fade_overlay(&g_world_screen_tint_tiles[g_menu_packet_buffer_index], g_gfx_screen_fade);
    world_camera_store_state_to_script_variables();
    menu_draw_active_window_frames();

    if (g_world_event_finish_check_pending != 0) {
        for (i = 1; i < 17; i++) {
            if (world_thread_is_running_asm(i) != 0) {
                break;
            }
        }
        if (i == 17) {
            op = script_get_current_scenario_finish_operation();
            if (op != 0) {
                if (script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
                    g_world_event_finish_check_pending = 0;
                    script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                    return 9;
                }
            } else {
                g_world_event_finish_check_pending = 0;
                g_menu_overlay_state = 0;
                return 0;
            }
            if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == EVENT_ID_FUNERAL) {
                return 0x13;
            }
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, ((op & 0xC00) >> 2) | (op & 0xFF));
            op = (op & 0xF300) >> 8;
            g_world_event_finish_check_pending = 0;
            g_menu_overlay_state = 0;
            if (op == 0x80) {
                return 9;
            }
            if (op == 0x81) {
                return 0xA;
            }
            if (op == 0x82) {
                return 0xB;
            }
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, 0);
            g_battle_entd_selection_mode = BATTLE_ENTD_SELECTION_MAP_ID;
            return 9;
        }
        g_world_event_finish_check_pending = 0;
    }

    script_update_event_input_state(buttons);
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        g_script_unfiltered_controller_input = g_script_event_input;
        script_apply_input_overrides(&g_script_event_input);
        g_world_script_saved_event_input = g_script_event_input;
        script_set_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);
    }
    if (g_world_map_reload_state >= 3) {
        g_world_map_reload_state = 0;
    }
    if (g_world_map_append_state >= 3) {
        g_world_map_append_state = 0;
        g_map_destruction_wait_state = 1;
    }
    g_world_event_pending_map_jump_out_id = -1;
    g_world_event_weather_request = -1;
    g_world_event_pending_map_state = -1;
    g_world_event_pending_map_jump_out_2_id = -1;
    g_world_event_pending_effect_id = -1;
    g_world_event_pending_loaded_evtchr_slot = -1;
    g_world_event_pending_evtchr_save_slot = -1;
    g_world_event_pending_saved_evtchr_clear_slot = -1;
    g_world_event_pending_loaded_evtchr_clear_slot = -1;
    g_world_event_pending_unit_vram_copy = -1;
    g_world_event_pending_reserved_vram_release_slot = -1;
    g_world_event_map_jump_in_request = 0;
    g_world_3d_object_use_request = 0;
    g_world_field_object_use_request = 0;

    if (g_world_companion_overlay_state == COMPANION_OVERLAY_STATE_JOBSTTS) {
        ordering_table = jobstts_entrypoint(g_menu_selected_unit.battle_id, 0);
        g_world_companion_overlay_prims = (s32)ordering_table;
        if (ordering_table != 0) {
            AddPrims(g_gfx_current_otag_entry, ordering_table, &ordering_table[0x3f]);
        } else {
            g_world_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        }
    }
    if (g_world_companion_overlay_state == COMPANION_OVERLAY_STATE_EQUIP) {
        prims = equip_entrypoint(g_menu_selected_unit.battle_id, g_gfx_current_otag_entry + 1);
        g_world_companion_overlay_prims = prims;
        if (prims >= 2) {
            AddPrims(g_gfx_current_otag_entry, (void*)prims, (void*)(prims + 0xFC));
        } else {
            g_world_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        }
    }

    script_load_event_input_state((menu_input_state_t*)&g_script_event_input);
    if (world_script_check_tutorial_event_slot() == 0 && g_script_event_input != PSX_PAD_SELECT
        && g_script_event_input != 0) {
        g_world_thread_change_cooldown = 5;
    }
    for (i = 1; i < 16; i++) {
        g_world_event_thread_status_snapshot[i] = world_thread_is_running_asm(i);
    }
    thread_yield();
    for (i = 1; i < 16; i++) {
        if (g_world_event_thread_status_snapshot[i] != world_thread_is_running_asm(i)) {
            g_world_thread_change_cooldown = 5;
        }
    }
    if (g_world_thread_change_cooldown != 0) {
        g_world_thread_change_cooldown--;
    }
    world_camera_load_state_from_script_variables();
    g_world_frame_counter++;
    g_menu_packet_buffer_index = (g_menu_packet_buffer_index + 1) & 1;
    if (g_world_menu_input_block_frames != 0) {
        g_world_menu_input_block_frames--;
    }
    script_frame_after_threads_stub();
    if (g_script_frame_result_override != 0) {
        return g_script_frame_result_override;
    }

    if (g_world_companion_overlay_state == COMPANION_OVERLAY_STATE_STUB) {
        world_menu_run_companion_overlay_stub();
        g_world_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
    } else if (g_world_companion_overlay_state == COMPANION_OVERLAY_STATE_BUNIT) {
        world_script_set_event_speed(1);
        i = bunit_entrypoint(0, 0);
        target_move_cursor_to_battle_id(i);
        g_menu_selected_unit.battle_id = i;
        g_world_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
    } else if (g_world_companion_overlay_state == COMPANION_OVERLAY_STATE_CARD_SAVE) {
        world_script_set_event_speed(1);
        polarity = g_main_gfx_screen_polarity;
        script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
        card_save_run_menu(0);
        g_world_companion_overlay_state = COMPANION_OVERLAY_STATE_IDLE;
        g_main_gfx_screen_polarity = polarity;
    }
    world_menu_open_companion_executable();
    world_menu_open_companion_data_file();
    world_script_process_pending_requests();
    script_frame_after_requests_stub();
    if (g_menu_input_mode == MENU_INPUT_BLOCKED) {
        return 1;
    }
    if (g_menu_input_mode == MENU_INPUT_TUTORIAL) {
        return 0;
    }
    for (i = 1; i < 17; i++) {
        if (world_thread_is_running_asm(i) != 0) {
            break;
        }
    }
    if (i == 17) {
        g_world_event_finish_check_pending = 1;
    }
    return 1;
}

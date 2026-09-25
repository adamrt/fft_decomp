#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/event.h"
#include "fft/main_gfx.h"
#include "fft/main_heap.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern void AddPrims(u32* ot, void* first, void* last); /* follows AddPrim; AddPrims-shaped call */
extern s32 jobstts_entrypoint(s32 unit);
extern s32 equip_entrypoint(s32 unit, void* otag);
extern s32 bunit_entrypoint(void);
extern void card_save_run_menu(s32 unused);

/* Run one frame of WORLD bookkeeping around the thread scheduler.
 *
 * Returns the scenario-finish result (9, 0xA, 0xB or 0x13, 0 when no finish
 * operation is pending) once every script thread has stopped, otherwise 1
 * (or g_world_frame_result_override while it is set, or 0 while menu input is disabled with state
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
    s32 polarity;

    g_battle_ai_workspace_ptr = g_battle_ai_workspace;
    if (g_world_gfx_draw_primitives_immediately != 0) {
        DrawSync(0);
        rect.w = 0x100;
        rect.x = 0;
        rect.h = 0xF0;
        rect.y = (g_main_gfx_screen_polarity & 1) * 0xF0;
        LoadImage(&rect, (u32*)g_main_heap_high_overlay_load_address);
    }
    g_world_gfx_active_otag_entries = ot;
    world_gfx_draw_screen_tint_tile(
        &g_world_screen_tint_tiles[g_world_menu_packet_buffer_index], g_world_script_tutorial_thread_switch_counter);
    world_noop_800f6ea8();
    world_menu_draw_active_window_frames();

    if (g_world_event_finish_check_pending != 0) {
        for (i = 1; i < 17; i++) {
            if (world_thread_is_running_80100164(i) != 0) {
                break;
            }
        }
        if (i == 17) {
            op = ((s32 (*)(void))world_script_get_current_scenario_finish_operation)();
            if (op != 0) {
                if (world_script_get_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS) != 0) {
                    g_world_event_finish_check_pending = 0;
                    world_script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 0);
                    return 9;
                }
            } else {
                g_world_event_finish_check_pending = 0;
                g_world_menu_overlay_state = 0;
                return 0;
            }
            if (world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) == 0x147) {
                return 0x13;
            }
            world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, ((op & 0xC00) >> 2) | (op & 0xFF));
            op = (op & 0xF300) >> 8;
            g_world_event_finish_check_pending = 0;
            g_world_menu_overlay_state = 0;
            if (op == 0x80) {
                return 9;
            }
            if (op == 0x81) {
                return 0xA;
            }
            if (op == 0x82) {
                return 0xB;
            }
            world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
            world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_ENTD, 0);
            g_battle_entd_selection_mode = 4;
            return 9;
        }
        g_world_event_finish_check_pending = 0;
    }

    world_input_update_menu_repeat(buttons);
    if (g_world_menu_input_disabled == 2) {
        g_world_input_unfiltered_controller = g_world_menu_new_button_input;
        world_menu_refresh_state_from_script_variables(&g_world_menu_new_button_input);
        g_world_script_saved_event_input = g_world_menu_new_button_input;
        world_script_set_variable(EVENT_SCRIPT_VAR_FORCED_CONTROLLER_INPUTS, 0);
    }
    if (g_world_map_reload_state >= 3) {
        g_world_map_reload_state = 0;
    }
    if (g_world_map_append_state >= 3) {
        g_world_map_append_state = 0;
        g_world_map_destruction_wait_state = 1;
    }
    g_world_event_pending_map_jump_out_id = -1;
    g_world_event_weather_request = -1;
    g_world_event_pending_map_state = -1;
    g_world_event_pending_map_jump_out_2_id = -1;
    g_world_event_pending_effect_id = -1;
    g_world_event_pending_loaded_evtchr_slot = (void*)-1;
    g_world_event_pending_evtchr_save_slot = -1;
    g_world_event_pending_saved_evtchr_clear_slot = -1;
    g_world_event_pending_loaded_evtchr_clear_slot = -1;
    g_world_event_pending_unit_vram_copy = -1;
    g_world_event_pending_reserved_vram_release_slot = -1;
    g_world_event_map_jump_in_request = 0;
    g_world_3d_object_use_request = 0;
    g_world_field_object_use_request = 0;

    if (g_world_companion_overlay_state == 3) {
        /* The target also passes a1 = 0 to the one-argument entry point. */
        g_world_companion_overlay_prims = ((s32 (*)(s32, s32))jobstts_entrypoint)(g_world_unit_view_battle_id, 0);
        if (g_world_companion_overlay_prims != 0) {
            AddPrims(g_world_gfx_active_otag_entries, (void*)g_world_companion_overlay_prims,
                (void*)(g_world_companion_overlay_prims + 0xFC));
        } else {
            g_world_companion_overlay_state = 0;
        }
    }
    if (g_world_companion_overlay_state == 4) {
        prims = equip_entrypoint(g_world_unit_view_battle_id, g_world_gfx_active_otag_entries + 1);
        g_world_companion_overlay_prims = prims;
        if (prims >= 2) {
            AddPrims(g_world_gfx_active_otag_entries, (void*)prims, (void*)(prims + 0xFC));
        } else {
            g_world_companion_overlay_state = 0;
        }
    }

    world_input_store_menu_state((world_menu_input_state_t*)&g_world_menu_new_button_input);
    if (world_script_check_tutorial_event_slot() == 0 && g_world_menu_new_button_input != PSX_PAD_SELECT
        && g_world_menu_new_button_input != 0) {
        g_world_thread_change_cooldown = 5;
    }
    for (i = 1; i < 16; i++) {
        g_world_event_thread_status_snapshot[i] = world_thread_is_running_80100164(i);
    }
    world_thread_yield();
    for (i = 1; i < 16; i++) {
        if (g_world_event_thread_status_snapshot[i] != world_thread_is_running_80100164(i)) {
            g_world_thread_change_cooldown = 5;
        }
    }
    if (g_world_thread_change_cooldown != 0) {
        g_world_thread_change_cooldown--;
    }
    world_noop_800f6ea0();
    g_world_frame_counter++;
    g_world_menu_packet_buffer_index = (g_world_menu_packet_buffer_index + 1) & 1;
    if (g_world_menu_input_block_frames != 0) {
        g_world_menu_input_block_frames--;
    }
    world_noop_800f1388();
    if (g_world_frame_result_override != 0) {
        return g_world_frame_result_override;
    }

    if (g_world_companion_overlay_state == 1) {
        world_noop_800f079c();
        g_world_companion_overlay_state = 0;
    } else if (g_world_companion_overlay_state == 2) {
        world_script_set_event_speed(1);
        /* The target passes a0 = a1 = 0 to the argument-less entry point. */
        i = ((s32 (*)(s32, s32))bunit_entrypoint)(0, 0);
        battle_target_move_cursor_to_battle_id(i);
        g_world_unit_view_battle_id = i;
        g_world_companion_overlay_state = 0;
    } else if (g_world_companion_overlay_state == 5) {
        world_script_set_event_speed(1);
        polarity = g_main_gfx_screen_polarity;
        world_script_set_variable(EVENT_SCRIPT_VAR_SAVE_IN_PROGRESS, 1);
        card_save_run_menu(0);
        g_world_companion_overlay_state = 0;
        g_main_gfx_screen_polarity = polarity;
    }
    world_process_bin_load_request();
    world_update_pending_file_load();
    world_noop_800f6e98();
    world_noop_800fdce8();
    if (g_world_menu_input_disabled == 1) {
        return 1;
    }
    if (g_world_menu_input_disabled == 2) {
        return 0;
    }
    for (i = 1; i < 17; i++) {
        if (world_thread_is_running_80100164(i) != 0) {
            break;
        }
    }
    if (i == 17) {
        g_world_event_finish_check_pending = 1;
    }
    return 1;
}

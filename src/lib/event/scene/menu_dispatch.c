/* Poll companion overlay requests and dispatch command-menu and help threads. */

#include "fft/battle_map.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/deploy.h"
#include "fft/file.h"
#include "fft/help.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_selection.h"
#include "fft/overlay.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/status_screen.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit.h"
#include "fft/unit_event.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Overlay load bookkeeping and disc tables. */
extern s32 g_overlay_companion_executable_loading;
extern s32 g_overlay_companion_executable_lbas[];
extern s32 g_overlay_companion_executable_sizes[];
extern s32 g_overlay_companion_executable_offsets[];
extern s32 g_overlay_companion_data_file_request_index;
extern s32 g_overlay_companion_data_file_loading;
extern s32 g_overlay_companion_data_file_lbas[];
extern s32 g_overlay_companion_data_file_sizes[];
extern u8* g_overlay_companion_data_file_buffer;

/* Menu threads and panel rendering. */
extern s32 g_menu_building_thread_started;
extern u8 g_menu_idle_action_window_x[];
extern u8 g_command_restore_unit_id;

/* Non-blocking two-step file load driven by g_thread_call_target:
 * issue the read for the pending request, then poll until it completes and
 * clear the request. */
void overlay_companion_file_poll(void) {
    if ((g_overlay_companion_data_file_request_index != 0) && (g_overlay_companion_data_file_loading == 0)) {
        g_thread_call_target = (void (*)(void))file_bytes_read_request;
        if (thread_call_on_main_stack(g_overlay_companion_data_file_lbas[g_overlay_companion_data_file_request_index],
                g_overlay_companion_data_file_sizes[g_overlay_companion_data_file_request_index],
                g_overlay_companion_data_file_buffer)
            == 0) {
            g_overlay_companion_data_file_loading = 1;
        }
    } else if ((g_overlay_companion_data_file_request_index != 0) && (g_overlay_companion_data_file_loading != 0)) {
        g_thread_call_target = file_load_state_get;
        if (thread_call_on_main_stack() == 0) {
            g_overlay_companion_data_file_loading = 0;
            g_overlay_companion_data_file_request_index = 0;
        }
    }
}

/* Per-frame driver of overlay_companion_request: start the
 * disc read of the requested overlay on the main stack, at its offset from the
 * companion overlay load address, then poll until the read finishes and clear the
 * request. */
void overlay_companion_poll(void) {
    s32 idx = g_overlay_companion_executable_request_index;

    if ((idx != 0) && (g_overlay_companion_executable_loading == 0)) {
        g_thread_call_target = (void (*)(void))file_bytes_read_request;
        if (thread_call_on_main_stack(g_overlay_companion_executable_lbas[idx],
                g_overlay_companion_executable_sizes[idx],
                g_overlay_companion_executable_offsets[idx] + (s32)g_heap_companion_overlay_load_address)
            == 0) {
            g_overlay_companion_executable_loading = 1;
        }
    } else if ((g_overlay_companion_executable_request_index != 0) && (g_overlay_companion_executable_loading != 0)) {
        g_thread_call_target = file_load_state_get;
        if (thread_call_on_main_stack() == 0) {
            g_overlay_companion_executable_loading = 0;
            g_overlay_companion_executable_request_index = 0;
        }
    }
}

/* Call command_reset_unit_records; nothing in BATTLE or WORLD calls this wrapper. */
void command_unit_records_reset(void) {
    command_reset_unit_records();
}

/* Empty: the overlay entry of COMPANION_OVERLAY_STATE_STUB. */
void overlay_companion_stub(void) {
}

/* Menu-building thread: once the previous menu, the camera and the target panel are done
 * (and a tutorial slot has started its next event), open menu parameter 1, the egg menu for
 * an egg, and reopen the skillset and ability lists when restoring the same unit's menu. */
static void entry_thread_start(void) {
    s32 menu_id = thread_current_parameter_1_get();
    s32 restore_depth;

    thread_inactive_wait(THREAD_SLOT_MENU);
    if (g_menu_overlay_state != 0) {
        thread_current_exit();
    }
    camera_idle_wait();
    while (thread_task_find(THREAD_TASK_ATTACK_TARGET_STATS) != 0) {
        thread_yield();
    }
    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        if (tutorial_is_current_event() != 0) {
            g_menu_input_mode = MENU_INPUT_TUTORIAL;
            script_variable_set(
                EVENT_SCRIPT_VAR_CURRENT_EVENT, script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) + 1);
            script_current_event_thread_start();
            thread_frames_wait(0x1E);
        }
        if (g_menu_input_mode != MENU_INPUT_PLAYER) {
            thread_frames_wait(2);
        }
    } else {
        thread_frames_wait(2);
    }
    tutorial_pulse_wait_value(SCRIPT_TUTORIAL_WAIT_MENU_ENTRY);
    if (menu_id == MENU_COMMAND_ID_OTHER_UNIT || menu_id == MENU_COMMAND_ID_OTHER_UNIT_INSPECT) {
        if (menu_selected_unit_stats_get()->unit_flags & UNIT_FLAG_EGG) {
            menu_id = MENU_COMMAND_ID_EGG;
        }
    }
    thread_start(THREAD_SLOT_MENU, g_menu_thread_menu_data[menu_id].thread_entry);
    thread_parameters_set(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[menu_id], 0, 0);
    restore_depth = g_menu_restore_depth;
    g_script_event_input = 0;
    if (g_menu_restore_pending != 0 && g_command_restore_unit_id == g_unit_panel_selected_unit.battle_id) {
        g_help_open = 1;
        thread_frames_wait(2);
        if (restore_depth > 0) {
            thread_start(THREAD_SLOT_MENU_CHILD, command_act_menu_thread);
            thread_parameters_set(
                THREAD_SLOT_MENU_CHILD, (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST], 0, 0);
        }
        thread_frames_wait(2);
        if (restore_depth >= 2) {
            thread_start(THREAD_SLOT_MENU_GRANDCHILD, command_ability_list_thread);
            thread_parameters_set(
                THREAD_SLOT_MENU_GRANDCHILD, (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST], 0, 0);
        }
        thread_frames_wait(2);
        g_menu_restore_pending = 0;
        g_help_open = 0;
    } else {
        g_menu_restore_depth = 0;
        g_command_restore_unit_id = g_unit_panel_selected_unit.battle_id;
    }
    thread_current_exit();
}

enum { IDLE_ACTION_MENU_ENTRY_COUNT = 0x34 };

/* Open an idle action menu (command_menu_e) for the active-turn unit: place the
 * entry windows beside the unit, restore the row the unit last chose and start
 * the menu-building thread on entry_thread_start.
 *
 * Nothing happens while menu input is blocked. The menu is also skipped while the
 * menus are suspended, a help window is open,
 * g_menu_building_thread_started is set (it is cleared here either way) or
 * menu_id is MENU_COMMAND_ID_NONE.
 *
 * g_menu_idle_action_window_x holds two window x bytes per entry, each
 * 0x80 less than the screen x; the second is used when the unit's projected x is
 * 0x101 or more, and a zero byte leaves the entry's x unchanged. WORLD only
 * clears the menu-building-started flag and does not read the menu id. */
void command_menu_open(s32 menu_id) {
#ifdef FFT_BATTLE
    SVECTOR screen_position;
    s32 i;
    s32 column;
    u8* window_x_table;
    menu_entry_t* entry;
    s16* selected_index;

    if (g_menu_input_mode != MENU_INPUT_BLOCKED) {
        if ((g_menu_overlay_state == 0) && (g_help_open == 0) && (g_menu_building_thread_started == 0)
            && (menu_id != MENU_COMMAND_ID_NONE)) {
            g_command_current_id = menu_id;
            g_menu_thread_menu_data = g_command_idle_action_entries;
            unit_screen_position_project(
                unit_battle_runtime_id_get(g_unit_panel_selected_unit.battle_id), &screen_position);
            i = 0;
            entry = g_menu_thread_menu_data;
            window_x_table = g_menu_idle_action_window_x;
            /* Two window x bytes per entry; a projected x of 0x101 or more uses the second. */
            column = screen_position.vx >= 0x101;
            do {
                /* The offset-plus-base integer sum is byte-required (addu operand order). */
                u8* window_x = (u8*)(column + (u32)window_x_table);

                column += 2;
                if (*window_x != 0) {
                    entry->window.x = *window_x + 0x80;
                }
                i += 1;
                entry += 1;
            } while (i < IDLE_ACTION_MENU_ENTRY_COUNT);
            command_restore_unit_selection(menu_id);
            selected_index = &g_menu_thread_menu_data[MENU_COMMAND_ID_STATUS_LIST].selected_index;
            *selected_index = 0;
            g_status_screen_status_list_layout.row_offset = 0;
            menu_clear_entry_selections();
            thread_start(THREAD_SLOT_MENU_BUILDING, entry_thread_start);
            thread_parameters_set(THREAD_SLOT_MENU_BUILDING, menu_id, 0, 0);
        }
        g_menu_building_thread_started = 0;
    }
#else
    g_menu_building_thread_started = 0;
#endif
}

/* Open the help menu on the menu-building thread slot unless input is held off, a modal overlay is
 * open, the camera moves, a companion overlay is busy or that slot runs. While a menu other than
 * the mini menu is up it also sets g_menu_building_thread_started, so the next idle action menu is
 * skipped. */
void help_menu_thread_start(void) {
    if (thread_is_running_asm(THREAD_SLOT_MENU) != 0 || thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        if (g_command_current_id != MENU_COMMAND_ID_MINI_MENU) {
            g_menu_building_thread_started = 1;
        }
    }
    if (g_script_input_suppression_frames != 0) {
        return;
    }
    if (g_menu_modal_open != 0) {
        return;
    }
    if (camera_is_active() != 0) {
        return;
    }
    if (g_overlay_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE) {
        return;
    }
    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        return;
    }
    g_help_open = 1;
    thread_start(THREAD_SLOT_MENU_BUILDING, help_menu_thread);
    thread_parameters_set(THREAD_SLOT_MENU_BUILDING, 0, 0, 0);
}

/* Request SMALL.OUT as the companion executable without waiting; overlay_small_poll drives the
 * load during map initialization. */
void overlay_small_request(void) {
    g_overlay_companion_executable_request_index = COMPANION_OVERLAY_SMALL;
}

/* Drive a pending companion overlay load during map initialization. Returns 1
 * while the request is still pending; once it completes the SMALL.OUT text
 * pointers are bound and it returns 0. With no request it returns 0 at once. */
s32 overlay_small_poll(void) {
    if (g_overlay_companion_executable_request_index == 0) {
        return 0;
    }
    overlay_companion_poll();
    if (g_overlay_companion_executable_request_index != 0) {
        return 1;
    }
    small_entrypoint();
    return 0;
}

/* Returns 0 in BATTLE and WORLD; in BATTLE it sits between
 * overlay_small_poll and battle_map_skip_deployment_thread. Nothing on the disc
 * calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
s32 battle_map_return_zero_1(void) {
    return 0;
}

/* Release the map initializer's deployment gate after one yield, then exit after another.
 * Map loading proceeds independently in the battle loop. */
void battle_map_skip_deployment_thread(void) {
    thread_yield();
    deploy_mark_finished();
    thread_yield();
    thread_current_exit();
}

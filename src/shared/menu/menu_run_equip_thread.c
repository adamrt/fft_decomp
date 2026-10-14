#include "fft/battle.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Run the EQUIP overlays, then restore the menu threads and icon-list cursors. */
void menu_run_equip_thread(void) {
    s32 saved;
    s32 i;
    s32 j;

    menu_set_modal_open();
    saved = g_menu_unit_view_mode;
    menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
    g_menu_overlay_state = 1;
    thread_wait_for_unit_view_panels();
    menu_request_open_companion_executable(COMPANION_OVERLAY_EQUIP);
    menu_reserve_high_overlay(0x20000);
    menu_request_open_companion_executable(COMPANION_OVERLAY_EQUIP_SECOND_HALF);
    thread_suspend(THREAD_SLOT_MENU);
    thread_suspend(THREAD_SLOT_MENU_CHILD);
    g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_EQUIP;
    do {
        g_script_event_input = 0;
        thread_yield();
    } while (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
    g_menu_thread_menu_data = g_battle_menu_idle_action_entries;
    g_battle_menu_action_slot_row_actions[0] = -3;
    g_sound_suppressed = 1;
    g_script_event_input |= PSX_PAD_CIRCLE;
    if (g_battle_companion_overlay_primitives == 0) {
        thread_resume(THREAD_SLOT_MENU);
        thread_resume(THREAD_SLOT_MENU_CHILD);
    }
    for (i = 9; i < 15; i++) {
        thread_set_parameters(i, 0, 0, 1);
    }
    thread_wait_frames(4);
    if (g_battle_companion_overlay_primitives != 0) {
        for (j = 0; j < 2; j++) {
            g_menu_pending_selection[j]
                = ((menu_entry_t*)g_thread_contexts[8 - j].function_parameter_1)->selected_index;
        }
    }
    menu_free_high_overlay();
    g_menu_overlay_state = 0;
    menu_start_unit_view_thread(saved, 0xFF, 0xFF);
    g_sound_suppressed = 0;
    g_script_event_input = 0;
    menu_clear_modal_open();
    thread_exit_current();
}

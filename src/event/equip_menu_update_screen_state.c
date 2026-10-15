#include "fft/event_equip.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Per-frame state machine of the EQUIP screen.
 *
 * Keeps the command menu thread (Equip / Remove / item list rows) running and
 * shows the highlighted row's help text. A chosen row parks the child slot
 * on an idle thread and enters that row's mode; result row 1 is refused with
 * the invalid sound. Each mode handler returns 0 when it is finished, which
 * returns to the command menu and suspends the child slot on the next call.
 * The screen closes when the command menu thread ends.
 */
void equip_menu_update_screen_state(void) {
    s32 result;
    s32 state;
    s32 mode;
    s16* value_ptr;

    if (g_equip_menu_thread_running == 0) {
        if (thread_is_running(EQUIP_THREAD_SLOT_COMMAND_MENU) == 0) {
            g_equip_command_menu.selected_index = 0;
            g_menu_thread_menu_data = &g_equip_command_menu;
            thread_start(EQUIP_THREAD_SLOT_COMMAND_MENU, menu_icon_list_thread);
            thread_set_parameters(EQUIP_THREAD_SLOT_COMMAND_MENU, (s32)g_menu_thread_menu_data, 0, 0);
            g_equip_thread_idle_stop_pending = 0;
            g_equip_menu_thread_running = 1;
        }
    }
    if (g_equip_thread_idle_stop_pending != 0) {
        thread_suspend(EQUIP_THREAD_SLOT_COMMAND_MENU_CHILD);
        g_equip_thread_idle_stop_pending = 0;
    }
    state = g_equip_menu_screen_state;
    if (state == EQUIP_SCREEN_STATE_COMMAND_MENU) {
        value_ptr = &g_equip_menu_result_rows[0];
        mode = *value_ptr;
        g_equip_text_help_message_id = g_equip_command_menu.selected_index + 0x1029;
        if (mode == 1) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
            equip_menu_reset_selection_indices();
        } else if (mode != -1) {
            thread_start(EQUIP_THREAD_SLOT_COMMAND_MENU_CHILD, thread_wait_forever_by_frames);
            g_equip_menu_screen_state = *value_ptr + EQUIP_SCREEN_STATE_EQUIP;
            menu_set_selection_value(0, 0);
            menu_set_selection_value(1, 0);
            menu_set_selection_value(2, 0);
            equip_menu_reset_selection_indices();
        }
        g_equip_menu_thread_running = thread_is_running(EQUIP_THREAD_SLOT_COMMAND_MENU);
        if (g_equip_menu_thread_running == 0) {
            g_equip_menu_screen_state = EQUIP_SCREEN_STATE_CLOSED;
        }
        return;
    }
    switch (state) {
    case EQUIP_SCREEN_STATE_EQUIP:
        result = equip_menu_run_equip_mode();
        break;
    case EQUIP_SCREEN_STATE_REMOVE:
        result = equip_menu_run_remove_mode();
        break;
    case EQUIP_SCREEN_STATE_ITEM_LIST:
        result = equip_menu_update_equipment();
        break;
    }
    if (result == 0) {
        g_equip_menu_screen_state = EQUIP_SCREEN_STATE_COMMAND_MENU;
        g_equip_thread_idle_stop_pending = 1;
    }
}

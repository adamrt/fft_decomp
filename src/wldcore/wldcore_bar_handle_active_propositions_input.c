#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* List-menu input step: cancel (0x40) and confirm (0x20) close the level
 * through WORLD thread 12, and 0x100 opens help text 0x1062. Once the thread
 * finishes, a confirmed level dispatches the selected row's value through
 * 0x8007af9c.
 *
 * The target keeps each `level->panel.result` store ahead of the `li $a0,5`
 * argument of the following cursor-state call, which lands in the jal delay
 * slot; without the empty barriers GCC schedules the li first.
 */
void wldcore_bar_handle_active_propositions_input(wldcore_menu_stack_record_t* level) {
    u32 buttons;
    s32 unused[2]; /* 8-byte frame slot the target reserves */

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_wldcore_menu_ordering_table_offset = 1;
        g_wldcore_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        if (level->panel.result != 0) {
            wldcore_proposition_push_cancel_confirm_level(
                level->panel.entries[g_wldcore_window_panel_render_state.selected_index]);
        } else {
            wldcore_menu_dispatch_resume_handler();
        }
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->panel.result = 0;
        /* Scheduling barrier: keeps the store ahead of the cursor argument. */
        __asm__("");
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_PROPOSITIONS);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    } else if (buttons & PSX_PAD_SELECT) {
        wldcore_bar_active_propositions_block_all_buttons(&level->panel);
        wldcore_menu_push_message_level(0x1062, 1);
    } else if (buttons & PSX_PAD_CIRCLE) {
        if (g_wldcore_list_row_flags[g_wldcore_window_panel_render_state.selected_index] != 0) {
            wldcore_sound_play_effect(MAIN_SFX_INVALID);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_RETURN_TOMORROW, 0);
        } else {
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            level->panel.result = 1;
            /* Scheduling barrier: keeps the store ahead of the cursor argument. */
            __asm__("");
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_PROPOSITIONS);
            g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        }
    }
}

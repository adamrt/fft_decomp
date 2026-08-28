#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

void wldcore_proposition_push_cancel_confirm_level(s32 value);

/* List-menu input step: cancel (0x40) and confirm (0x20) close the level
 * through WORLD thread 12, and 0x100 opens help text 0x1062. Once the thread
 * finishes, a confirmed level dispatches the selected row's value through
 * 0x8007af9c.
 *
 * The target keeps each `level->confirmed` store ahead of the `li $a0,5`
 * argument of the following cursor-state call, which lands in the jal delay
 * slot; without the empty barriers GCC schedules the li first.
 */
void wldcore_bar_handle_active_propositions_input(wldcore_bar_active_propositions_level_t* level) {
    u32 buttons;
    s32 unused[2]; /* 8-byte frame slot the target reserves */

    if ((g_main_system_flags & 4) && world_thread_is_running(0xC) == 0) {
        g_wldcore_menu_ordering_table_offset = 1;
        g_wldcore_menu_result = 0;
        g_main_system_flags ^= 4;
        g_wldcore_menu_stack_depth--;
        if (level->confirmed != 0) {
            wldcore_proposition_push_cancel_confirm_level(
                level->values[g_wldcore_window_panel_render_state.selected_index]);
        } else {
            wldcore_menu_dispatch_resume_handler();
        }
        return;
    }
    buttons = g_wldcore_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->confirmed = 0;
        /* Scheduling barrier: keeps the store ahead of the cursor argument. */
        __asm__ volatile("");
        wldcore_list_clear_cursor_state(5);
        g_main_system_flags |= 4;
        world_thread_set_parameters(0xC, 0, 0, 1);
    } else if (buttons & PSX_PAD_SELECT) {
        wldcore_bar_active_propositions_clear_result((wldcore_menu_panel_level_t*)level);
        wldcore_menu_push_message_level(0x1062, 1);
    } else if (buttons & PSX_PAD_CIRCLE) {
        if (g_wldcore_list_row_flags[g_wldcore_window_panel_render_state.selected_index] != 0) {
            wldcore_sound_play_effect(MAIN_SFX_INVALID);
            world_thread_set_parameters(0xE, 0x19, 0xB81C, 0);
        } else {
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            level->confirmed = 1;
            /* Scheduling barrier: keeps the store ahead of the cursor argument. */
            __asm__ volatile("");
            wldcore_list_store_cursor_state(5);
            g_main_system_flags |= 4;
            world_thread_set_parameters(0xC, 0, 0, 1);
        }
    }
}

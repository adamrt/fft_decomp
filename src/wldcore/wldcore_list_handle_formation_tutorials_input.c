#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* List level input step for cursor slot 18 (help message 0x108b). A confirmed
 * entry starts fade 2 and thread 12's closing animation; once it finishes the
 * level switches to state 2, sets system flag 0x40 and starts thread 14. When
 * thread 14 ends, the selected index is marked in the bitset at 0x8004c6bc,
 * menu brightness is restored and the formation screen opens for it. */
void wldcore_list_handle_formation_tutorials_input(wldcore_menu_panel_level_t* level) {
    if (level->result == 2) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
            s16* selected = &g_wldcore_window_panel_render_state.selected_index;

            wldcore_set_bit_value(g_main_tutorial_progress_masks, *selected, 1);
            world_menu_set_brightness(0x80, 0x80, 0x80);
            world_formation_run_screen_for_menu_index(*selected);
            g_wldcore_menu_ordering_table_offset = 1;
            g_menu_blocked_buttons = 0;
            g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED | MAIN_SYSTEM_FLAG_DISPLAY_OFF;
            g_wldcore_menu_stack_depth--;
            wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_FORMATION_TUTORIALS, 0, 0);
        }
        return;
    }
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_FADING) {
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_FORMATION_TUTORIALS);
            level->result = 2;
            g_main_system_flags |= MAIN_SYSTEM_FLAG_DISPLAY_OFF;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
            return;
        }
        g_wldcore_menu_ordering_table_offset = 1;
        g_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_FORMATION_TUTORIALS);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_formation_tutorials_block_all_buttons(level);
        wldcore_menu_push_message_level(0x108B, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
    }
}

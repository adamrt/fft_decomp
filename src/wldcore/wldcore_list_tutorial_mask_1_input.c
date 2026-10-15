/* Mask-1 tutorial list button blocks and input step. */

#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

void wldcore_list_tutorial_mask_1_restore_blocked_buttons(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}

void wldcore_list_tutorial_mask_1_block_all_buttons(wldcore_menu_panel_level_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of a list level that hands the chosen entry to the world script.
 *
 * State 2 waits for thread 14, then marks the selected entry in tutorial
 * progress mask 1, sets script variables 0x1fc and 0x27 and requests the event
 * through system flag 0x200000. While flags & 0xc is 4, a confirmed choice
 * starts thread 14 (state 2) and a canceled one pops the level. Otherwise
 * cancel (0x40) and confirm (0x20) start thread 12 and record the choice;
 * 0x100 on a non-empty list opens help message 0x108c. */
void wldcore_list_handle_tutorial_mask_1_entries_input(wldcore_menu_panel_level_t* level) {
    s16* selected;

    if (level->result == 2) {
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
            selected = &g_wldcore_window_panel_render_state.selected_index;
            wldcore_set_bit_value(&g_main_tutorial_progress_masks[1], *selected, 1);
            script_set_variable(EVENT_SCRIPT_VAR_SUPPRESS_PROGRESS_EFFECTS, 1);
            script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, g_wldcore_tutorial_event_ids[*selected]);
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_MASK_1_ENTRIES);
            g_main_saved_list_cursor_state = g_wldcore_tutorial_entries_cursor_state;
            g_main_system_flags = (g_main_system_flags | MAIN_SYSTEM_FLAG_TUTORIAL_EVENT) ^ MAIN_SYSTEM_FLAG_RUNNING;
            g_main_saved_weather_variable = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        }
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && !(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
        if (level->result != 0) {
            g_main_system_flags |= MAIN_SYSTEM_FLAG_DISPLAY_OFF;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
            level->result = 2;
            return;
        }
        g_wldcore_menu_ordering_table_offset = 1;
        g_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_MASK_1_ENTRIES);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_tutorial_mask_1_block_all_buttons(level);
        wldcore_menu_push_message_level(0x108C, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
    }
}

#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* List level input step for cursor slot 3 (help message 0x105f); confirmed
 * entries are handed to 0x800732b8. Confirm is refused with message 0xb80e
 * when g_main_save_proposition_count has reached 8. When script variable
 * 0x360 + entry has bit 8 set, the parent level's windows and render record
 * are hidden, message 0xb85c is shown and the level waits in state 2 for
 * confirm or cancel. */
void wldcore_list_handle_propositions_input(wldcore_menu_panel_level_t* level) {
    s32 depth;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        wldcore_list_propositions_close_level(level);
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_AVAILABLE_PROPOSITIONS);
            wldcore_proposition_push_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_AVAILABLE_PROPOSITIONS);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (level->result == 2) {
        if (g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            level->result = 1;
            g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        }
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_propositions_block_all_buttons(level);
        wldcore_menu_push_message_level(0x105F, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        if (g_main_save_proposition_count >= 8) {
            wldcore_sound_play_effect(MAIN_SFX_INVALID);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_NO_MORE_JOBS, 0);
            return;
        }
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        if (script_get_variable(
                (g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK)
                + EVENT_SCRIPT_VAR_PROPOSITION_FIRST)
            & WLDCORE_PROPOSITION_VARIABLE_ATTEMPTED) {
            depth = g_wldcore_menu_stack_depth;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.first_window].flags
                |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.second_window]
                .flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2]
                                                .window_pair_render.render_index]
                .flags |= WORLD_RENDER_RECORD_HIDDEN;
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_TOLD_BEFORE, 0);
            level->result = 2;
            return;
        }
        level->result = 1;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

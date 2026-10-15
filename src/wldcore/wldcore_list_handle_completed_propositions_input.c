#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Handle input for the job-report confirmation level. */
void wldcore_list_handle_completed_propositions_input(wldcore_menu_stack_record_t* level) {
    s32 buttons;
    s32 depth;
    point32_t origin;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_wldcore_menu_ordering_table_offset = 1;
        g_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        wldcore_menu_dispatch_resume_handler();
        return;
    }

    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_COMPLETED_PROPOSITIONS);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        wldcore_list_completed_propositions_block_all_buttons(level);
        wldcore_menu_push_message_level(0x1061, 1);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        wldcore_list_completed_propositions_block_all_buttons(level);
        wldcore_proposition_prepare_report(
            level->proposition_list.entries[g_wldcore_window_panel_render_state.selected_index]);
        wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_COMPLETED_PROPOSITIONS);
        g_text_substitution_values[0] = g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index];
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_HEAR_ABOUT_JOB, 0);
        origin.x = 0x48;
        origin.y = 0x20;
        wldcore_window_build_yes_no_panel(origin);
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS_BROWSE;
        g_wldcore_menu_stack_depth = depth + 1;
        g_wldcore_menu_stack_records_next[depth].proposition_report_confirm.argument
            = level->proposition_list.entries[g_wldcore_window_panel_render_state.selected_index];
    }
}

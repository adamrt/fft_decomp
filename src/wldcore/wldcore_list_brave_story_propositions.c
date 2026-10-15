#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Opens a panel list of the script variables 0x360-0x3bf with bit 0x04 set
 * (entry values 0x6000 + index), shows up to eight rows at (0x4c, 0x50) with
 * width 0x96, appends its 0x5c-byte record tagged 0x29 and hides the parent
 * list window's content render record. Twin of wldcore_list_open_story_events. */
void wldcore_list_open_brave_story_propositions(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32 count;
    s32 i;
    s32 record_index;

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    count = 0;
    i = 0;
    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    for (; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST) & WLDCORE_PROPOSITION_VARIABLE_COMPLETED) {
            *values = i + TEXT_ID_COMPLETED_PROPOSITION_NAME_BASE;
            *flags = 0;
            flags++;
            values++;
            count++;
        }
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x96;
    origin.x = 0x4C;
    layout.visible_rows = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_BRAVE_STORY_PROPOSITIONS, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 8;
    record_index = g_wldcore_menu_stack_depth;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index - 1].list_window.content_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_types_next[record_index] = WLDCORE_MENU_LEVEL_BRAVE_STORY_PROPOSITIONS;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* The g_wldcore_menu_resume_handlers entry of WLDCORE_MENU_LEVEL_BRAVE_STORY_PROPOSITIONS: nothing to restore. */
void wldcore_menu_resume_brave_story_propositions_level(void) {
}

/* The g_wldcore_menu_publish_handlers entry of WLDCORE_MENU_LEVEL_BRAVE_STORY_PROPOSITIONS: nothing to publish. */
void wldcore_menu_publish_brave_story_propositions_level(void) {
}

/* List level input step for cursor slot 16 (the list opened by 0x80087b94):
 * twin of wldcore_list_handle_unexplored_lands_input with help message 0x105b and no file-loading gate.
 * Closing without a selection re-flags the list window's content render
 * record (0x100) before returning to the bar menu; a confirmed entry goes
 * to 0x80087fa4. */
void wldcore_list_handle_brave_story_propositions_input(wldcore_menu_panel_level_t* level) {
    s32 depth;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        depth = g_wldcore_menu_stack_depth;
        g_menu_blocked_buttons = 0;
        g_wldcore_menu_ordering_table_offset = 1;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth = depth - 1;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_BRAVE_STORY_PROPOSITIONS);
            wldcore_menu_push_proposition_attempt_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.content_render].flags
            |= WORLD_RENDER_RECORD_POP_IN;
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_BRAVE_STORY_PROPOSITIONS);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_menu_push_message_level(0x105B, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

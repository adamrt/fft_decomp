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

#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

void wldcore_list_open_script_flags_01a4_01bb(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32 count;
    s32 i;
    s32 record_index;

    count = 0;
    i = 0;
    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    for (; i < EVENT_SCRIPT_VAR_FACT_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_FACT_FIRST) != 0) {
            *values = i + (s16)TEXT_ID_FACT_NAME_BASE;
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
    layout.text_width = 0x8C;
    origin.x = 0x58;
    layout.visible_rows = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_SCRIPT_FLAGS_01A4_01BB, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .list_window.content_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    record_index = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_SCRIPT_FLAGS_01A4_01BB;
    g_wldcore_menu_stack_depth = record_index + 1;
}

#include "fft/thread.h"
#include "fft/wldcore.h"

/* Opens the detail view (menu level type 0x20) for a script-variable list
 * entry selected in 0x8008368c: value is variable index * 16 + variable
 * value. It loads the chapter data of the variable's group of 16, appends a
 * render record, hides the parent list window's records and starts WORLD
 * thread 14 with text 0xb868 (variable 0, value substituted) or 0xb849.
 *
 * The unused 48-byte local reproduces the target's frame size. */
void wldcore_menu_push_variable_detail_level(s32 value) {
    u8 unused[0x30];
    s32 variable;
    s32 index;
    s32 depth;

    variable = value >> 4;
    wldcore_load_message_block_if_changed(variable / 16 + 6);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.value = value;
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.render_index = index;
    g_wldcore_window_render_records[index].flags
        = (g_wldcore_window_render_records[index].flags & ~WORLD_RENDER_RECORD_8BIT_CLUT) | WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.main_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.side_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .list_window.frame_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.upper_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.lower_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    if (variable == 0) {
        g_text_substitution_values[0] = (value & 0xF) + WLDCORE_TEXT_ID_RAMZA_TITLE_FIRST;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_RAMZA_INFO, 0);
    } else {
        g_text_substitution_values[0] = value + TEXT_ID_PERSON_PROFILE_NAME_BASE;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    }
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].variable_detail.phase = WLDCORE_DETAIL_PHASE_LOAD;
    g_wldcore_menu_stack_records_next[depth].variable_detail.timer = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_VARIABLE_DETAIL;
    g_wldcore_menu_stack_depth = depth + 1;
}

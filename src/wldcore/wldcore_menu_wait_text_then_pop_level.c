#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

void wldcore_menu_wait_text_then_pop_level(wldcore_menu_message_level_t* level) {
    s32* index = &g_wldcore_scroll_text_render_record_index;

    if (g_wldcore_window_render_records[*index].flags & 0x100) {
        return;
    }
    if (wldcore_text_is_window_finished((wldcore_text_scrollable_window_t*)((u8*)index - 0x44)) != 0) {
        return;
    }
    g_wldcore_current_button_input = 0;
    g_wldcore_menu_result = level->saved_menu_result;
    g_wldcore_window_record_count -= 1;
    g_wldcore_window_render_object_count -= 1;
    g_wldcore_menu_stack_depth -= 1;
    if (level->reload_text != 0) {
        world_text_restore_section_pointers();
    }
    wldcore_menu_dispatch_resume_handler();
}

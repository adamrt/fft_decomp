#include "fft/wldcore.h"

/* Clamp a saved cursor slot to the current list and restore it as active. */
void wldcore_list_clamp_cursor_state(s32 index, s32 count, s32 visible) {
    s32 end;

    if (count < g_wldcore_menu_cursor_states[index].selected_index + 1) {
        g_wldcore_menu_cursor_states[index].selected_index = count - 1;
        g_wldcore_menu_cursor_states[index].scroll_offset = count - visible;
    }
    end = g_wldcore_menu_cursor_states[index].scroll_offset;
    end += visible;
    if (count - end < 0) {
        g_wldcore_menu_cursor_states[index].scroll_offset = count - visible;
    }
    {
        s32 value;

        value = g_wldcore_menu_cursor_states[index].selected_index;
        g_wldcore_window_panel_render_state.selected_index = value;
        value = g_wldcore_menu_cursor_states[index].scroll_offset;
        g_wldcore_window_panel_render_state.select_text_table = value;
    }
}

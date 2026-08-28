#include "fft/wldcore.h"

/* Save the active menu position in a persistent cursor slot. */
void wldcore_list_store_cursor_state(s32 index) {
    g_wldcore_menu_cursor_states[index].selected_index = g_wldcore_window_panel_render_state.selected_index;
    g_wldcore_menu_cursor_states[index].scroll_offset = g_wldcore_window_panel_render_state.select_text_table;
}

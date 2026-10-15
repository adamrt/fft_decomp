#include "fft/wldcore.h"

/* Reset the window palette and both map-cursor scroll axes. The scroll fields
 * preserve the original store order after the indexed window store. */
void wldcore_window_reset_state_and_scroll(wldcore_menu_stack_record_t* level) {
    g_wldcore_window_records[level->window_index].palette = 10;
    g_wldcore_map_cursor_scroll_y.value = 0;
    g_wldcore_map_cursor_scroll_x.value = 0;
    g_wldcore_map_cursor_scroll_y.output = 0;
    g_wldcore_map_cursor_scroll_x.output = 0;
}

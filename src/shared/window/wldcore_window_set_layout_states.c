#include "fft/wldcore.h"

/* Select the palettes for the active-proposition list and its four windows. */
void wldcore_window_set_layout_states(wldcore_menu_proposition_list_level_t* level) {
    g_wldcore_window_records[level->cursor_window].palette = 10;
    g_wldcore_window_records[level->title_window].palette = g_wldcore_window_records[level->count_window].palette
        = g_wldcore_window_records[level->days_window].palette = 6;
    g_wldcore_window_render_records[level->render_index].palette = 2;
}

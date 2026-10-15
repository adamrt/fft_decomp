#include "fft/wldcore.h"

/* Restore the proposition detail renderer's palette. */
void wldcore_window_restore_proposition_detail_palette(wldcore_menu_stack_record_t* level) {
    g_wldcore_window_render_records[level->window_render.render_index].palette = 0;
}

#include "fft/wldcore.h"

/* Restore the location list's optional renderer palette. */
void wldcore_window_restore_location_list_palette(wldcore_menu_stack_record_t* level) {
    if (level->location_list.extra_render != -1) {
        g_wldcore_window_render_records[level->location_list.extra_render].palette = 0;
    }
}

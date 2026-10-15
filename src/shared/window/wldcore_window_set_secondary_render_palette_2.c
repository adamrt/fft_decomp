#include "fft/wldcore.h"

/* Sets the second owned render record's palette selector when its slot is present. */
void wldcore_window_set_secondary_render_palette_2(wldcore_menu_slot_set_t* slots) {
    if (slots->render_b != -1) {
        g_wldcore_window_render_records[slots->render_b].palette = 2;
    }
}

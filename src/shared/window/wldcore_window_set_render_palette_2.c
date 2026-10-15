#include "fft/wldcore.h"

/* Select palette 2 for the window's render record. */
void wldcore_window_set_render_palette_2(wldcore_window_render_pair_t* pair) {
    g_wldcore_window_render_records[pair->render_index].palette = 2;
}

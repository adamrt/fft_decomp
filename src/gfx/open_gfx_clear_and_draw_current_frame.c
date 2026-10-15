#include "fft/open.h"

/* Start a frame: clear the active ordering table and queue the overlay fade. */
void open_gfx_clear_and_draw_current_frame(void) {
    open_gfx_clear_current_otag();
    open_gfx_update_fade_overlay(g_open_gfx_otags[g_active_graphics_buffer_index]);
}

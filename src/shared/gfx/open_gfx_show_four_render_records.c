#include "fft/open.h"

/* Show the four title menu entries. */
void open_gfx_show_four_render_records(open_title_controller_t* title) {
    g_open_gfx_render_records_36[title->records_36[1]].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_open_gfx_render_records_36[title->records_36[2]].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_open_gfx_render_records_36[title->records_36[3]].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_open_gfx_render_records_36[title->records_36[4]].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
}

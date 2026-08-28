#include "fft/open.h"

void open_gfx_hide_four_render_records(const open_birthday_render_record_indices_t* group) {
    g_open_gfx_render_records_36[group->indices[0]].flags &= ~OPEN_RENDER_RECORD_VISIBLE;
    g_open_gfx_render_records_36[group->indices[1]].flags &= ~OPEN_RENDER_RECORD_VISIBLE;
    g_open_gfx_render_records_36[group->indices[2]].flags &= ~OPEN_RENDER_RECORD_VISIBLE;
    g_open_gfx_render_records_36[group->indices[3]].flags &= ~OPEN_RENDER_RECORD_VISIBLE;
}

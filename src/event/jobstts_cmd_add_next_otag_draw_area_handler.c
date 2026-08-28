#include "fft/jobstts.h"

const u8* jobstts_cmd_add_next_otag_draw_area_handler(const u8* data) {
    if (g_jobstts_gfx_otag_index_locked == 0) {
        g_jobstts_gfx_draw_area.y = 0;
        jobstts_gfx_enqueue_draw_area(&g_jobstts_gfx_draw_area, g_jobstts_gfx_otag_index + 1);
    }
    return data + data[1];
}

#include "fft/jobstts.h"
#include "psx/types.h"

u8* jobstts_cmd_release_otag_lock_handler(u8* data) {
    u16 otag_index;
    if (g_jobstts_gfx_otag_index_locked != 0) {
        otag_index = (u16)g_jobstts_gfx_otag_index + 1;
        g_jobstts_gfx_draw_area.y = 0;
        g_jobstts_gfx_otag_index = otag_index;
        jobstts_gfx_enqueue_draw_area(&g_jobstts_gfx_draw_area, (s16)otag_index);
        g_jobstts_gfx_otag_index_locked = 0;
        g_jobstts_gfx_otag_index = (u16)g_jobstts_gfx_otag_index + 1;
    }
    return data + data[1];
}

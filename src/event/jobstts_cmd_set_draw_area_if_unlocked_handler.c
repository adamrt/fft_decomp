#include "fft/jobstts.h"
#include "psx/types.h"

u8* jobstts_cmd_set_draw_area_if_unlocked_handler(u8* data) {
    s16 area[4];

    if (g_jobstts_gfx_otag_index_locked == 0) {
        area[0] = data[2];
        area[1] = data[3];
        area[2] = data[4];
        area[3] = data[5];
        jobstts_gfx_enqueue_draw_area((RECT*)area, g_jobstts_gfx_otag_index - 1);
    }
    return data + data[1];
}

#include "fft/jobstts.h"

/* Draw the border tiles through a vertically expanding transition clip. */
u8* jobstts_cmd_draw_border_tiles_with_transition_clip(u8* command) {
    u8* next;
    RECT rect;

    if (g_jobstts_gfx_transition_frame < 8) {
        s32 scale;

        if (g_jobstts_gfx_transition_frame < 3) {
            g_jobstts_cmd_stream_input = 0;
        }
        scale = g_jobstts_gfx_transition_height_percent[g_jobstts_gfx_transition_frame];
        rect.w = 0x100;
        rect.h = (command[6] * scale) / 100;
        rect.x = 0;
        rect.y = (command[4] + (command[6] >> 1)) - (rect.h >> 1);
        /* The target passes a third argument the callee does not take. */
        ((void (*)(RECT*, s16, s16))jobstts_gfx_enqueue_draw_area)(&rect, g_jobstts_gfx_otag_index, rect.y);
        g_jobstts_gfx_otag_index_locked = 1;
        g_jobstts_gfx_otag_index = g_jobstts_gfx_otag_index + 1;
    } else {
        g_jobstts_gfx_otag_index_locked = 0;
    }
    next = jobstts_create_border_tiles(command);
    g_jobstts_gfx_otag_index++;
    return next;
}

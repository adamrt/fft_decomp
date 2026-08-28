#include "fft/jobstts.h"
#include "psx/types.h"

void jobstts_cmd_run_stream(u8* data, s32 flags) {
    g_jobstts_gfx_otag_index_locked = 0;
    g_jobstts_gfx_otag_index = 0;
    g_jobstts_gfx_semitransparency = 0;
    g_jobstts_cmd_stream_input = flags;
    while (*data != 0x16) {
        data = g_jobstts_cmd_handlers[*data](data);
    }
    if (g_jobstts_gfx_transition_frame < 10) {
        g_jobstts_gfx_transition_frame = g_jobstts_gfx_transition_frame + 1;
    }
}

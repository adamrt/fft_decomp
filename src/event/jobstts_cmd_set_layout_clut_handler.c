#include "fft/event_jobstts.h"
#include "psx/types.h"

u8* jobstts_cmd_set_layout_clut_handler(u8* cmd) {
    if (g_jobstts_text_layout_mode != 0) {
        g_jobstts_gfx_clut_id = GetClut(cmd[6] << 4, cmd[7] | (cmd[5] << 8));
    } else {
        g_jobstts_gfx_clut_id = GetClut(cmd[3] << 4, cmd[4] | (cmd[2] << 8));
    }
    return cmd + cmd[1];
}

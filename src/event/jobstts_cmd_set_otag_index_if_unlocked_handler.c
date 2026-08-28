#include "fft/jobstts.h"
#include "psx/types.h"

const u8* jobstts_cmd_set_otag_index_if_unlocked_handler(const u8* data) {
    if (g_jobstts_gfx_otag_index_locked == 0) {
        g_jobstts_gfx_otag_index = data[3];
    }
    return data + data[1];
}

#include "fft/jobstts.h"
#include "psx/types.h"

const u8* jobstts_cmd_set_semitrans_handler(const u8* data) {
    g_jobstts_gfx_semitransparency = data[3];
    return data + data[1];
}

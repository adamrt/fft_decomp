#include "fft/jobstts.h"
#include "psx/types.h"

void jobstts_bits_init_primary_reader(const u8* data) {
    g_jobstts_bits_primary_reset = 1;
    g_jobstts_bits_primary_source = data;
}

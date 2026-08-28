#include "fft/jobstts.h"
#include "psx/types.h"

void jobstts_bits_init_secondary_reader(const u8* data) {
    g_jobstts_bits_secondary_reset = 1;
    g_jobstts_bits_secondary_source = data;
}

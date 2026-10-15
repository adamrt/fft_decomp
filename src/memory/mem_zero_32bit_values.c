#include "fft/memory.h"

/* Zero byte_count / 4 32-bit values starting at dst. */
void mem_zero_32bit_values(void* destination, s32 byte_count) {
    s32* dst = destination;
    s32 i;

    byte_count /= 4;
    for (i = 0; i < byte_count; i++) {
        *dst++ = 0;
    }
}

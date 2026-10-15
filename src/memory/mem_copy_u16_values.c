#include "fft/memory.h"

/* Copy byte_count / 2 16-bit values; an odd last byte is left alone. */
void mem_copy_u16_values(void* destination, const void* source, s32 byte_count) {
    u16* dst = destination;
    const u16* src = source;
    s32 count = byte_count / 2;
    s32 i;
    for (i = 0; i < count; i++) {
        *dst++ = *src++;
    }
}

#include "fft/memory.h"

void mem_copy_bytes_source_first(const void* source, void* destination, int count) {
    const u8* src = source;
    u8* dst = destination;
    int i;

    for (i = 0; i < count; i++) {
        *dst++ = *src++;
    }
}

#include "fft/main_heap.h"

void main_util_copy_byte_data(const void* source, void* destination, s32 count) {
    const u8* src = source;
    u8* dst = destination;
    s32 index = 0;

    while (index < count) {
        *dst++ = *src++;
        index++;
    }
}

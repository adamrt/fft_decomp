#include "fft/main.h"
#include "fft/memory.h"
#include "psx/types.h"

void mem_zero(void* destination, s32 count) {
    u8* dst = destination;
    s32 index = 0;

    while (index < count) {
        *dst++ = 0;
        index++;
    }
}

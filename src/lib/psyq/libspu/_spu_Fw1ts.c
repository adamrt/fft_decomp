/* SCUS_942.21 0x800193a4..0x80019403. */
#include "psx/libspu.h"

/* Short busy-wait (240 multiply steps) that gives the SPU time between register accesses. */
void _spu_Fw1ts(void) {
    /* Both stack variables provide the delay; the multiplier wraps modulo 2^32. */
    volatile s32 count;
    volatile u32 value;
    value = 13;
    for (count = 0; count < 240; count++) {
        value *= 3;
    }
}

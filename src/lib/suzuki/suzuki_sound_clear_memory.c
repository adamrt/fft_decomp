#include "psx/suzuki.h"

/* Zeroes size bytes: 16-byte blocks, then 32-bit values, then bytes. */
void suzuki_sound_clear_memory(void* destination, s32 size) {
    u32* words = destination;
    u8* bytes;
    s32 count;

    for (count = size >> 4; count != 0; count--) {
        words[0] = words[1] = words[2] = words[3] = 0;
        words += 4;
    }
    for (count = (size >> 2) & 3; count != 0; count--) {
        *words++ = 0;
    }
    bytes = (u8*)words;
    for (count = size & 3; count != 0; count--) {
        *bytes++ = 0;
    }
}

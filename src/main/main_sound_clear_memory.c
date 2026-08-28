#include "psx/types.h"

/* Zeroes size bytes: 16-byte blocks, then words, then bytes. */
void main_sound_clear_memory(void* destination, s32 size) {
    u32* words;
    u8* bytes;
    s32 count;

    words = destination;
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

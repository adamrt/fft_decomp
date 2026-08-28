#include "psx/types.h"

/* Copies size bytes: 16-byte blocks, then words, then bytes.
 *
 * The byte tail keeps advancing the word pointers through casts; separate
 * byte pointers get their own registers and an extra copy. */
void main_sound_copy_memory(void* destination, void* source, s32 size) {
    u32* dst;
    u32* src;
    s32 count;
    u32 word0;
    u32 word1;
    u32 word2;
    u32 word3;

    dst = destination;
    src = source;
    for (count = size >> 4; count != 0; count--) {
        word0 = src[0];
        word1 = src[1];
        word2 = src[2];
        word3 = src[3];
        dst[0] = word0;
        dst[1] = word1;
        dst[2] = word2;
        dst[3] = word3;
        src += 4;
        dst += 4;
    }
    for (count = (size >> 2) & 3; count != 0; count--) {
        *dst++ = *src++;
    }
    for (count = size & 3; count != 0; count--) {
        *(u8*)dst = *(u8*)src;
        dst = (u32*)((u8*)dst + 1);
        src = (u32*)((u8*)src + 1);
    }
}

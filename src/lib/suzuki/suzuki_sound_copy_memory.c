#include "fft/suzuki.h"

/* Copies size bytes: 16-byte blocks, then 32-bit values, then bytes. */
void suzuki_sound_copy_memory(void* destination, const void* source, s32 size) {
    u8* dst = destination;
    const u8* src = source;
    s32 count;
    u32 word0;
    u32 word1;
    u32 word2;
    u32 word3;

    for (count = size >> 4; count != 0; count--) {
        const u32* source_words = (const u32*)src;
        u32* destination_words = (u32*)dst;
        word0 = source_words[0];
        word1 = source_words[1];
        word2 = source_words[2];
        word3 = source_words[3];
        destination_words[0] = word0;
        destination_words[1] = word1;
        destination_words[2] = word2;
        destination_words[3] = word3;
        src += 16;
        dst += 16;
    }
    for (count = (size >> 2) & 3; count != 0; count--) {
        *(u32*)dst = *(const u32*)src;
        dst += sizeof(u32);
        src += sizeof(u32);
    }
    for (count = size & 3; count != 0; count--) {
        *dst++ = *src++;
    }
}

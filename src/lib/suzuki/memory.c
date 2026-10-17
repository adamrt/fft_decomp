/* Memory copy and clear helpers, and Suzuki SPU allocation. */

#include "fft/suzuki.h"
#include "psx/libspu.h"

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

/* Empty. suzuki_sound_init calls it after setting up the driver heap. */
void suzuki_sound_init_stub(void) {
}

/* Reserve SPU RAM through the SDK allocator. */
s32 suzuki_sound_alloc_spu_ram(s32 size) {
    return SpuMalloc(size);
}

/* Release SPU RAM and report success. */
s32 suzuki_sound_free_spu_ram(u32 addr) {
    SpuFree(addr);
    return 0;
}

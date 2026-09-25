#include "psx/types.h"

/* Suzuki resource header: word 2 is the total size in bytes, including the
 * 8-byte header. */
typedef struct main_sound_smd_resource {
    u8 unknown_00[8];
    u32 words[1];
} main_sound_smd_resource_t;

/* Sums every word from the size field to the end of the resource. */
s32 main_sound_sum_resource_words(main_sound_smd_resource_t* resource) {
    u32* word;
    s32 count;
    s32 sum;

    word = resource->words;
    count = (resource->words[0] - 8) >> 2;
    sum = 0;
    do {
        sum += *word++;
    } while (--count != 0);
    return sum;
}

#include "fft/suzuki.h"

/* Sums every 32-bit value from the size field to the end of the resource. */
s32 suzuki_sound_sum_resource_words(suzuki_resource_prefix_t* resource) {
    u32* word = &resource->size_bytes;
    s32 count = (resource->size_bytes - 8) >> 2;
    s32 sum = 0;

    do {
        sum += *word++;
    } while (--count != 0);
    return sum;
}

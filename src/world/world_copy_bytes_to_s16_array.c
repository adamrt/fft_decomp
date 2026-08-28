#include "psx/types.h"

void world_copy_bytes_to_s16_array(s16* destination, const u8* source, s32 count) {
    s32 i;

    for (i = 0; i < count; i++) {
        *destination = *source;
        source++;
        destination++;
    }
}

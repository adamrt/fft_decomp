#include "psx/types.h"

void world_fill_16_bytes_fe(u8* buf) {
    s32 value = 0xFE;
    s32 i = 0xF;

    do {
        *buf = (u8)value;
        i -= 1;
        buf += 1;
    } while (i >= 0);
}

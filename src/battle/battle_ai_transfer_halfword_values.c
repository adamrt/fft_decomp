#include "psx/types.h"

void battle_ai_transfer_halfword_values(u16* dst, u16* src, s32 byte_count) {
    s32 count = byte_count / 2;
    s32 i;
    for (i = 0; i < count; i++) {
        *dst++ = *src++;
    }
}

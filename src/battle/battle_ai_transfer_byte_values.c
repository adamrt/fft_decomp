#include "fft/battle.h"

void battle_ai_transfer_byte_values(u8* dst, const u8* src, s32 len) {
    s32 end;
    if (len > 0) {
        end = len + (s32)dst;
        do {
            *dst = *src;
            dst = dst + 1;
            src = src + 1;
        } while ((s32)dst < end);
    }
}

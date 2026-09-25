#include "fft/battle.h"

/* Zero byte_count / 4 words starting at dst. */
void battle_ai_clear_words(s32* dst, s32 byte_count) {
    s32 i;

    byte_count /= 4;
    for (i = 0; i < byte_count; i++) {
        *dst++ = 0;
    }
}

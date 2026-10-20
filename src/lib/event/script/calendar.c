/* Store the packed month and day flags written by the event UnlockDate instruction. */

#include "fft/script.h"
#include "psx/types.h"

/* Set or clear one flag in a 32-bit-word bitmap. */
static void flag_assign(u32* bits, s32 index, s32 set) {
    s32 word = index / 32;
    u32 mask = 1U << (index & 0x1F);

    if (set != 0) {
        bits[word] |= mask;
    } else {
        bits[word] &= ~mask;
    }
}

/* Store five day bits and four month bits in the indexed nine-bit date record. */
void script_opcode_85_unlock_date(u32* bits, s32 record, s32 month, s32 day) {
    s32 base = record * 9;
    s32 i;
    u8 mask = 1;

    for (i = 0; i < 5; i++) {
        flag_assign(bits, base + i, day & mask);
        mask <<= 1;
    }
    mask = 1;
    for (i = 0; i < 4; i++) {
        s32 bit = i + 5;
        flag_assign(bits, base + bit, month & mask);
        mask <<= 1;
    }
}

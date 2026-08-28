#include "fft/battle.h"

/* UnlockDate event instruction: store day (5 bits) then month (4 bits) in
 * the 9-bit date slot date_index of bitset. */
void battle_script_unlockdate_event_instruction(s32 bitset, s32 date_index, s32 month, s32 day) {
    s32 first_bit;
    s32 i;
    s32 mask;

    first_bit = date_index * 9;
    mask = 1;
    i = 0;
    do {
        battle_script_assign_bitset_flag((u32*)bitset, first_bit + i, day & (u8)mask);
        i++;
        mask *= 2;
    } while (i < 5);
    mask = 1;
    i = 0;
    do {
        s32 slot = i + 5;
        battle_script_assign_bitset_flag((u32*)bitset, first_bit + slot, month & (u8)mask);
        i++;
        mask *= 2;
    } while (i < 4);
}

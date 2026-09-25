#include "fft/battle.h"
#include "psx/types.h"

s32 battle_effect_get_timing_curve_value(battle_effect_timing_entry_t* entry) {
    u16 packed_index;
    s32 packed_value;
    s32 offset;

    packed_index = entry->curve_index;
    offset = (s16)packed_index / 2;
    packed_value = g_battle_effect_nibble_table[offset];
    if (packed_index & 1) {
        packed_value >>= 4;
    }
    return packed_value & 0xf;
}

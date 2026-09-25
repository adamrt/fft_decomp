#include "fft/battle.h"

void battle_effect_load_lba_and_size(s32 effect_id, s32* lba, s32* byte_length) {
    volatile u32 stack_padding[2];

    *lba = g_battle_effect_disc_entries[effect_id].sector;
    *byte_length = g_battle_effect_disc_entries[effect_id].size;
}

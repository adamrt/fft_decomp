#include "psx/types.h"

typedef struct battle_effect_disc_entry {
    s32 lba;
    s32 byte_length;
} battle_effect_disc_entry_t;

extern battle_effect_disc_entry_t g_battle_effect_disc_entries[];

void battle_effect_load_lba_and_size(s32 effect_id, s32* lba, s32* byte_length) {
    volatile u32 stack_padding[2];

    *lba = g_battle_effect_disc_entries[effect_id].lba;
    *byte_length = g_battle_effect_disc_entries[effect_id].byte_length;
}

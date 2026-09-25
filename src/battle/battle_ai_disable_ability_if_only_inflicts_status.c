#include "fft/battle.h"

/*
 * Disable an AI ability that does nothing except add one unwanted status.
 *
 * The halfword update spans skillset and usage_flags so GCC emits the target's
 * single lhu/andi/sh sequence while clearing only the usable bit. A direct
 * status_infliction pointer folds +7 into its base; byte-stepping the typed AI
 * container retains the target's base pointer and named field access.
 */
void battle_ai_disable_ability_if_only_inflicts_status(battle_ai_ability_entry_t* entry, s32 status_id) {
    if ((g_battle_ai_considered_ability_flags.word & BATTLE_AI_ABILITY_ADD_STATUS) == 0) {
        return;
    }
    if ((g_battle_ai_considered_ability_flags_1 & 0xEE) != 0) {
        return;
    }

    {
        s32 status_set;
        s32 mask;
        battle_ai_data_t* cursor;
        s32 shift;

        status_set = 0;
        mask = 0x80;
        cursor = &g_battle_ai_data_base;
        shift = status_id;
        for (; status_set < BATTLE_STATUS_BYTE_COUNT; status_set++, shift -= 8) {
            if (status_set == status_id / 8) {
                if ((cursor->considered_ability.parameters.bytes.status_infliction[0] & ((mask >> shift) ^ 0xFF))
                    != 0) {
                    return;
                }
                /* A separate increment per branch keeps the target's cursor
                 * base; one shared increment folds the field offset into it. */
                cursor = (battle_ai_data_t*)((u8*)cursor + 1);
                continue;
            }
            if (cursor->considered_ability.parameters.bytes.status_infliction[0] != 0) {
                return;
            }
            cursor = (battle_ai_data_t*)((u8*)cursor + 1);
        }
    }
    entry->skillset_flags.packed &= 0x7FFF;
}

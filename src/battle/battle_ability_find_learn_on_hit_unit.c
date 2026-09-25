#include "fft/battle.h"
#include "psx/types.h"

enum {
    BATTLE_LEARN_ON_HIT_ROLL_MAXIMUM = 100,
    BATTLE_LEARN_ON_HIT_ROW_BYTES = 3,
    BATTLE_LEARN_ON_HIT_ABILITY_COUNT = 0x18,
    BATTLE_LEARNED_ABILITIES_OFFSET = 0x99
};

/* Find a unit that learns the ability it was just hit with.
 *
 * Every hit unit whose ability outcome is still pending is demoted to outcome 4
 * first.  Monsters, units whose status prevents a reaction, and units that
 * already know the ability are skipped; the remaining candidates roll the
 * ability's learn rate out of 100.  Returns the learning unit's slot and writes
 * the ability id through `ability_id`, or -1 when nobody learns it. */
s32 battle_ability_find_learn_on_hit_unit(s32 attacker_id, u16* ability_id) {
    u8 known[BATTLE_LEARN_ON_HIT_ROW_BYTES];
    ability_data_t* ability;
    battle_stats_t* unit;
    s32 i;
    s32 j;
    s32 row;
    s32 byte_index;
    s32 bit;
    u8* scratch;
    u8* src;
    s32 k;
    u8 skillset;

    if (g_current_ability_hamedo_flag != 0) {
        return -1;
    }
    if (g_battle_acting_unit_used_ability_id == 0) {
        return -1;
    }

    ability = &g_main_ability_data[g_battle_acting_unit_used_ability_id];
    i = 0;
    if (!(ability->type_flags & ABILITY_TYPE_FLAG_LEARN_ON_HIT)) {
        return -1;
    }

    scratch = known;
    do {
        if (attacker_id != i) {
            unit = &g_battle_unit_stats[i];
            if (unit->ability_outcome & 2) {
                unit->ability_outcome = 4;
                if (!(unit->unit_flags & UNIT_FLAG_MONSTER)
                    && main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PREVENT_REACTION) == 0) {
                    skillset = unit->primary_skillset;
                    row = unit->job_id;
                    if (row < JOB_ID_SQUIRE) {
                        row = 0;
                    } else {
                        row = row - JOB_ID_SQUIRE;
                    }
                    /* Typed field access reassociates the 0x99 field offset
                     * with the row and demonstrably breaks the byte-exact
                     * address calculation. Preserve the target's word-row,
                     * unit-base, then field-offset order. */
                    src = (u8*)((u32)(row * BATTLE_LEARN_ON_HIT_ROW_BYTES) + (u32)unit
                        + BATTLE_LEARNED_ABILITIES_OFFSET);
                    for (k = 0; k < BATTLE_LEARN_ON_HIT_ROW_BYTES; k++) {
                        scratch[k] = src[k];
                    }

                    for (j = 0; j < BATTLE_LEARN_ON_HIT_ABILITY_COUNT; j++) {
                        byte_index = j / 8;
                        bit = 0x80 >> (j - byte_index * 8);
                        if (scratch[byte_index] & bit) {
                            continue;
                        }
                        if ((main_ability_get_id_from_skillset(skillset, j) & 0xffff)
                            != g_battle_acting_unit_used_ability_id) {
                            continue;
                        }
                        if (main_util_roll_pass_fail(BATTLE_LEARN_ON_HIT_ROLL_MAXIMUM, ability->learn_rate) == 0) {
                            *ability_id = g_battle_acting_unit_used_ability_id;
                            return i;
                        }
                    }
                }
            }
        }
        i++;
    } while (i < BATTLE_UNIT_SLOT_COUNT);
    return -1;
}

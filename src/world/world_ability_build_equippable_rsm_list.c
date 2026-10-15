#include "fft/main.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Builds a sorted, unique list of Reaction, Support, or Movement abilities
 * that a formation unit can equip.
 *
 * The list includes abilities learned in the unit's base job and generic jobs
 * Chemist through Dancer. Squire appears only when it is the base job, and
 * Mime is excluded. The -1-terminated result marks the current job's innate
 * support ability unavailable so the menu rejects it. The slot values are 1
 * for Reaction, 2 for Support, and 3 for Movement.
 *
 * `n` is the bubble-sort bound and then the ability being filtered; the two
 * uses share one register.
 */
s32 world_ability_build_equippable_rsm_list(s16 unit_index, s32 slot, s16* out) {
    s16 abilities[204];
    s32 count;
    s32 i;
    s32 n;
    s32 swap;
    s32 last;
    s16 job_id = g_world_unit_pointers[unit_index]->identity.job_id;

    count = ability_build_list_variant(
        unit_index, job_get_base_variant(unit_index), slot, abilities, WORLD_ABILITY_LIST_MODE_LEARNED_ONLY);
    for (i = 1; i < JOB_ID_GENERIC_COUNT - 1; i++) {
        count += ability_build_list_variant(
            unit_index, JOB_ID_SQUIRE + i, slot, &abilities[count], WORLD_ABILITY_LIST_MODE_LEARNED_ONLY);
    }

    for (n = count - 1; n > 0; n--) {
        i = 0;
        if (n > 0) {
            do {
                if (abilities[i] >= abilities[i + 1]) {
                    swap = abilities[i];
                    abilities[i] = abilities[i + 1];
                    abilities[i + 1] = swap;
                }
                i++;
            } while (i < n);
        }
    }

    last = ABILITY_LIST_ENTRY_END;
    count = 0;
    for (i = 0; abilities[i] != ABILITY_LIST_ENTRY_END; i++) {
        n = abilities[i];
        if (n != last) {
            if (job_id == JOB_ID_CHEMIST && n == ABILITY_ID_SUPPORT_THROW_ITEM) {
                n = ABILITY_LIST_ENTRY_DISABLED | ABILITY_ID_SUPPORT_THROW_ITEM;
            }
            if (job_id == JOB_ID_MONK && n == ABILITY_ID_SUPPORT_MARTIAL_ARTS) {
                n = ABILITY_LIST_ENTRY_DISABLED | ABILITY_ID_SUPPORT_MARTIAL_ARTS;
            }
            if (job_id == JOB_ID_MEDIATOR && n == ABILITY_ID_SUPPORT_MONSTER_TALK) {
                n = ABILITY_LIST_ENTRY_DISABLED | ABILITY_ID_SUPPORT_MONSTER_TALK;
            }
            if (job_id == JOB_ID_NINJA && n == ABILITY_ID_SUPPORT_TWO_SWORDS) {
                n = ABILITY_LIST_ENTRY_DISABLED | ABILITY_ID_SUPPORT_TWO_SWORDS;
            }
            out[count++] = n;
            last = abilities[i];
        }
    }
    out[count] = ABILITY_LIST_ENTRY_END;
    return count;
}

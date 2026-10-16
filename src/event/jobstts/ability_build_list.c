/* The selected job's ability list for one menu category, optionally only the unlearned abilities. */

#include "fft/bits.h"
#include "fft/data_ability.h"
#include "fft/data_skillset.h"
#include "fft/event_jobstts.h"
#include "fft/job.h"
#include "fft/main_unit.h"
#include "fft/unit.h"
#include "psx/types.h"

/* Fill `list` with the abilities of job `job_id`'s skillset in `ability_category` for unit
 * `unit_index`, end it with ABILITY_LIST_ENTRY_END and return the count. A skillset that no
 * job owns falls back to the unit's primary skillset.
 *
 * Abilities the unit has not learned carry ABILITY_LIST_ENTRY_DISABLED, plus
 * ABILITY_LIST_ENTRY_HIDE_DETAILS when JP cannot learn them. A unit whose current job is a
 * special monster counts every ability as learned. A nonzero `unlearned_only` omits the
 * learned abilities, so an empty result means the job is mastered.
 */
s32 jobstts_ability_build_list(
    s16 unit_index, s16 job_id, menu_ability_category_e ability_category, s16* list, s32 unlearned_only) {
    s16* abilities;
    s16 generic_index;
    s32 count;
    s32 learned;
    s16 ability_id;
    s16 encoded_ability_id;
    s32 byte_offset;
    jobstts_unit_job_data_t* unit_data;
    s32 i; /* holds the skillset id before it counts the two loops */
    s16 lo;
    s16 hi;

    generic_index = job_get_generic_index(job_id);
    i = job_get_skillset(job_id);
    if (job_find_first_for_skillset(i) == -1) {
        i = g_jobstts_unit_data[unit_index]->primary_skillset;
    }
    abilities = (s16*)main_ability_store_skillset_abilities(i, SKILLSET_ABILITY_FILTER_ALL);

    if (ability_category == MENU_ABILITY_CATEGORY_ACTION) {
        lo = 1;
        hi = ABILITY_ID_REACTION_FIRST - 1;
    } else if (ability_category == MENU_ABILITY_CATEGORY_REACTION) {
        lo = ABILITY_ID_REACTION_FIRST;
        hi = ABILITY_ID_SUPPORT_FIRST - 1;
    } else if (ability_category == MENU_ABILITY_CATEGORY_SUPPORT) {
        lo = ABILITY_ID_SUPPORT_FIRST;
        hi = ABILITY_ID_MOVEMENT_FIRST - 1;
    } else if (ability_category == MENU_ABILITY_CATEGORY_MOVEMENT) {
        lo = ABILITY_ID_MOVEMENT_FIRST;
        hi = ABILITY_ID_RANDOM_FIRST - 1;
    } else {
        lo = 1;
        hi = ABILITY_ID_RANDOM_FIRST - 1;
    }

    /* Blank the skillset entries outside the category's ability id range. */
    for (i = 0; i < SKILLSET_ABILITY_LIST_COUNT; i++) {
        if (abilities[i] < lo || hi < abilities[i]) {
            abilities[i] = 0;
        }
    }

    count = 0;
    unit_data = g_jobstts_unit_data[unit_index];
    /* The job's learned-ability bits, one per skillset slot, read most significant first.
     * Keeping this offset separate preserves the target's GCC operand order;
     * direct learned_abilities indexing reverses the final commutative addu. */
    byte_offset = generic_index * UNIT_LEARNED_ABILITY_BYTES_PER_JOB
        + offsetof(jobstts_unit_job_data_t, career.learned_abilities);
    bits_init_primary_reader((u8*)(byte_offset + (s32)unit_data));
    for (i = 0; i < SKILLSET_ABILITY_LIST_COUNT; i++) {
        if (job_is_special_monster(g_jobstts_unit_data[unit_index]->job_id) != 0) {
            learned = 1;
        } else {
            learned = bits_read_primary(1);
        }

        ability_id = abilities[i];
        if (ability_id != 0) {
            encoded_ability_id = ability_id;
            if (learned == 0) {
                if ((g_main_ability_data[ability_id].type_flags >> ABILITY_TYPE_FLAG_CANNOT_LEARN_WITH_JP_SHIFT) == 0) {
                    encoded_ability_id |= ABILITY_LIST_ENTRY_DISABLED;
                } else {
                    encoded_ability_id |= ABILITY_LIST_ENTRY_DISABLED | ABILITY_LIST_ENTRY_HIDE_DETAILS;
                }
                list[count] = encoded_ability_id;
                count++;
            }
            if (unlearned_only == 0 && learned != 0) {
                list[count] = encoded_ability_id;
                count++;
            }
        }
    }

    list[count] = ABILITY_LIST_ENTRY_END;
    return count;
}

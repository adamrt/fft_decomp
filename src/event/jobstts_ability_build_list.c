#include "fft/event_jobstts.h"

/* Build the selected job's ability list for one menu category.
 *
 * A nonzero unlearned_only value omits learned abilities, allowing the caller
 * to detect a mastered job from an empty result. Unlearned entries carry a
 * display flag that distinguishes whether they can be learned with JP.
 */
s32 jobstts_ability_build_list(
    s16 unit_index, s16 job_id, jobstts_ability_category_e ability_category, s16* list, s32 unlearned_only) {
    s16* abilities;
    s16* ability_cursor;
    s16 generic_index;
    s32 skillset;
    s32 count;
    s32 kind;
    s32 ability_id;
    s32 encoded_ability_id;
    s32 byte_offset;
    jobstts_unit_job_data_t* unit_data;
    /* Pin: unpinned GCC swaps $s0 and $s1 between `i` and the output cursor. */
    register s32 i __asm__("$16");
    s32 lo;
    s32 hi;
    s32 filter_lo;
    s32 filter_hi;
    s16 scratch[4];

    generic_index = jobstts_job_get_generic_index(job_id);
    skillset = jobstts_job_get_skillset(job_id);
    if (jobstts_job_find_first_for_skillset(skillset) == -1) {
        skillset = (u16)g_jobstts_unit_data[unit_index]->primary_skillset;
    }
    abilities = (s16*)main_ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ALL);

    if (ability_category == JOBSTTS_ABILITY_CATEGORY_ACTION) {
        lo = 1;
        hi = ABILITY_ID_REACTION_FIRST - 1;
    } else if (ability_category == JOBSTTS_ABILITY_CATEGORY_REACTION) {
        lo = ABILITY_ID_REACTION_FIRST;
        hi = ABILITY_ID_SUPPORT_FIRST - 1;
    } else if (ability_category == JOBSTTS_ABILITY_CATEGORY_SUPPORT) {
        lo = ABILITY_ID_SUPPORT_FIRST;
        hi = ABILITY_ID_MOVEMENT_FIRST - 1;
    } else if (ability_category == JOBSTTS_ABILITY_CATEGORY_MOVEMENT) {
        lo = ABILITY_ID_MOVEMENT_FIRST;
        hi = ABILITY_ID_RANDOM_FIRST - 1;
    } else {
        lo = 1;
        hi = ABILITY_ID_RANDOM_FIRST - 1;
    }

    i = 0;
    filter_lo = lo;
    filter_hi = hi;
    ability_cursor = abilities;
    do {
        if (*ability_cursor < filter_lo || filter_hi < *ability_cursor) {
            *ability_cursor = 0;
        }
        ability_cursor++;
        i++;
    } while (i < SKILLSET_ABILITY_LIST_COUNT);

    count = 0;
    i = 0;
    unit_data = g_jobstts_unit_data[unit_index];
    /* Keeping this offset separate preserves the target's GCC operand order;
     * direct learned_abilities indexing reverses the final commutative addu. */
    byte_offset = generic_index * 3 + 0xb;
    jobstts_bits_init_primary_reader((u8*)(byte_offset + (s32)unit_data));
    do {
        if (jobstts_job_is_special_monster(g_jobstts_unit_data[unit_index]->job_id) != 0) {
            kind = 1;
        } else {
            kind = jobstts_bits_read_primary(1);
        }

        ability_id = *abilities;
        if (ability_id != 0) {
            encoded_ability_id = ability_id;
            /* Keeps the copy distinct so it fills the beqz delay slot. */
            __asm__("" : "=r"(ability_id) : "0"(ability_id));
            if (kind == 0) {
                if ((g_main_ability_data[ability_id].type_flags >> ABILITY_TYPE_FLAG_CANNOT_LEARN_WITH_JP_SHIFT) == 0) {
                    encoded_ability_id |= ABILITY_LIST_ENTRY_DISABLED;
                } else {
                    encoded_ability_id |= ABILITY_LIST_ENTRY_DISABLED | ABILITY_LIST_ENTRY_HIDE_DETAILS;
                }
                list[count] = encoded_ability_id;
                count++;
            }
            if (unlearned_only == 0 && kind != 0) {
                list[count] = encoded_ability_id;
                count++;
            }
        }
        abilities++;
        i++;
    } while (i < SKILLSET_ABILITY_LIST_COUNT);

    list[count] = ABILITY_LIST_ENTRY_END;
    return count;
}

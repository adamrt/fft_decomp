/* Unit List job availability and ability-list construction. */

#include "fft/ability.h"
#include "fft/ability_list.h"
#include "fft/bits.h"
#include "fft/job.h"
#include "fft/job_list.h"
#include "fft/skillset.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_list.h"
#include "fft/unit_party.h"
#include "psx/types.h"

/* Map Squire through Mime to zero-based indexes; unique jobs map to zero. */
s32 job_generic_index_get(s32 job_id) {
    u16 id = job_id;
    s16 result;

    if (id >= JOB_ID_SQUIRE && id <= JOB_ID_MIME) {
        result = id;
        result -= JOB_ID_SQUIRE;
    } else {
        result = 0;
    }
    return result;
}

/* Return the lowest job ID with this skillset, or -1. */
s32 job_first_for_skillset_find(s32 skillset) {
    s32 i;

    for (i = 0; i < JOB_ID_COUNT; i++) {
        if (job_data_get(i)->skillset == skillset) {
            return i;
        }
    }
    return -1;
}

/* The job that heads a unit's job list: a monster's own job, Squire for a
 * generic human identity, and for a special character the identity value
 * itself, which is that character's job id. */
s32 job_list_base_get(s16 unit_id) {
    unit_list_record_t* unit = g_unit_list_pointers[unit_id];
    u32 job_id = unit->character_identity;

    if (job_id == CHARACTER_IDENTITY_MONSTER) {
        job_id = unit->job_id;
    } else if (job_id >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
        job_id = JOB_ID_SQUIRE;
    }
    return job_id;
}

/* Store generic-job unlocks in the party record's big-endian byte order. */
void job_calculate_and_store_unlocks(party_unit_t* party, u8 unlocked_jobs[UNIT_UNLOCKED_JOB_BYTE_COUNT]) {
    s32 jobs
        = job_unlocks_calculate(party->career.job_levels, party->gender_flags & (UNIT_FLAG_FEMALE | UNIT_FLAG_MALE));
    unlocked_jobs[0] = jobs >> 16;
    unlocked_jobs[1] = jobs >> 8;
    unlocked_jobs[2] = jobs;
}

/*
 * Writes a unit's selectable jobs to a -1-terminated list and returns their
 * count.
 *
 * The base job comes first, followed by the generic jobs whose unlocked bit
 * is set (Bard excluded for female units, Dancer for male units); special
 * monsters and units using monster skillsets get only the base job. The
 * third argument is not read.
 */
s32 unit_list_build_job_list(s32 unit_id, s16* job_ids, s32 unused_2) {
    s32 saved_unit_id = unit_id;
    u32 unit_index = (u8)saved_unit_id;
    unit_list_record_t** unit_table = g_unit_list_pointers;
    unit_list_record_t** slot;
    s16* out;
    s16* job_ids_output;
    unit_list_record_t* unit;
    unit_list_record_t** unit_data_cursor;
    s32 job_count;
    s32 generic_job_index;

    slot = &unit_table[unit_index];
    unit = *slot;
    job_ids_output = job_ids;
    bits_init_reader(unit->unlocked_jobs);
    job_ids_output[0] = job_list_base_get(saved_unit_id & 0xff);
    job_count = 1;
    if (job_is_special_monster((*slot)->job_id) || (*slot)->uses_monster_skillset != 0) {
        job_ids_output[1] = -1;
        return 1;
    }

    /* Skip Squire's bit: the base job above takes Squire's place. */
    bits_read(1);
    generic_job_index = 1;
    unit_data_cursor = slot;
    out = job_ids_output + 1;
    for (; generic_job_index < JOB_ID_GENERIC_COUNT; generic_job_index++) {
        s32 job_is_unlocked;

        job_is_unlocked = bits_read(1);
        /* saved_unit_id now holds the job id; a separate local changes the allocation. */
        saved_unit_id = generic_job_index + JOB_ID_SQUIRE;
        if (saved_unit_id == JOB_ID_BARD && ((*unit_data_cursor)->gender_flags & UNIT_FLAG_FEMALE) != 0) {
            continue;
        }
        if (saved_unit_id == JOB_ID_DANCER && ((*unit_data_cursor)->gender_flags & UNIT_FLAG_MALE) != 0) {
            continue;
        }
        if (job_is_unlocked != 0) {
            *out = saved_unit_id;
            out++;
            job_count++;
        }
    }
    job_ids_output[job_count] = -1;
    return job_count;
}

/* Return the skillset of a job from the job data table. */
s32 job_skillset_get(s32 job_id) {
    return job_data_get(job_id)->skillset;
}

/*
 * For unit `unit_index` and job `job_id`'s skillset (the unit's primary skillset
 * when no job owns it), ABILITY_LIST_MODE_VIEW_ALL fills `list` with the abilities
 * in `ability_category`, marking unlearned ones ABILITY_LIST_ENTRY_DISABLED (plus
 * ABILITY_LIST_ENTRY_HIDE_DETAILS when JP cannot learn them), ends it with
 * ABILITY_LIST_ENTRY_END and returns the count; ABILITY_LIST_MODE_COUNT_UNLEARNED
 * only counts the unlearned abilities. The other modes list nothing. A special
 * monster counts every ability as learned, and a unit on a monster skillset gets
 * only the first three, all learned.
 */
s32 ability_list_build(s16 unit_index, s16 job_id, menu_ability_category_e ability_category, s16* list, s32 mode) {
    s32 skillset = job_skillset_get(job_id);
    s16 generic_index = job_generic_index_get(job_id);
    s16* abilities;
    s32 i;
    s32 count;
    s16 lo;
    s16 hi;
    s32 learned;
    s16 ability_id;
    s16 encoded_ability_id;
    s32 byte_offset;
    unit_list_record_t* unit;

    if (job_first_for_skillset_find(skillset) == -1) {
        skillset = g_unit_list_pointers[unit_index]->abilities[0];
    }
    abilities = (s16*)ability_skillset_list_build(skillset, SKILLSET_ABILITY_FILTER_ALL);

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
    unit = g_unit_list_pointers[unit_index];
    /* The job's learned-ability bits, one per skillset slot. Integer address arithmetic
     * preserves the final ADDU operand order. */
    byte_offset = generic_index * UNIT_LEARNED_ABILITY_BYTES_PER_JOB + offsetof(unit_list_record_t, learned_abilities);
    bits_init_reader((const u8*)(byte_offset + (s32)unit));
    for (i = 0; i < SKILLSET_ABILITY_LIST_COUNT; i++) {
        if (job_is_special_monster(g_unit_list_pointers[unit_index]->job_id)) {
            learned = 1;
        } else if (g_unit_list_pointers[unit_index]->uses_monster_skillset != 0) {
            if (i >= 3) {
                break;
            }
            learned = 1;
        } else {
            learned = bits_read(1);
        }

        ability_id = abilities[i];
        if (ability_id != 0) {
            encoded_ability_id = ability_id;
            if (mode == ABILITY_LIST_MODE_VIEW_ALL) {
                if (learned == 0) {
                    if ((g_ability_data[ability_id].type_flags >> ABILITY_TYPE_FLAG_CANNOT_LEARN_WITH_JP_SHIFT) == 0) {
                        encoded_ability_id |= ABILITY_LIST_ENTRY_DISABLED;
                    } else {
                        encoded_ability_id |= ABILITY_LIST_ENTRY_DISABLED | ABILITY_LIST_ENTRY_HIDE_DETAILS;
                    }
                }
                list[count] = encoded_ability_id;
                count++;
            } else if (mode == ABILITY_LIST_MODE_COUNT_UNLEARNED && learned == 0) {
                count++;
            }
        }
    }

    if (mode != ABILITY_LIST_MODE_COUNT_UNLEARNED) {
        list[count] = ABILITY_LIST_ENTRY_END;
    }
    return count;
}

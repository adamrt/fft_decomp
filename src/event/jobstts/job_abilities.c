/* Status unit job records and job/ability-list construction. */

#include "fft/ability.h"
#include "fft/ability_list.h"
#include "fft/bits.h"
#include "fft/job.h"
#include "fft/job_list.h"
#include "fft/skillset.h"
#include "fft/status_screen.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_runtime.h"
#include "psx/libc.h"
#include "psx/types.h"

static status_screen_unit_jobs_t g_status_screen_unit_job_data = { 0 };

/* Copy the battle unit's job data into the compact job-screen working record.
 *
 * Units on a team other than the player's, and special monsters, keep their stored
 * unlock bits; the player's team (team_kind 0, or 2 for a guest without the
 * player-control flag) has them recalculated from job levels and gender. Current and
 * total JP are capped at UNIT_JOB_POINT_MAX, the four digits the menu shows. */
static void status_screen_copy_unit_jobs(unit_stats_t* unit, status_screen_unit_jobs_t* out) {
    s32 i;
    s32 team_kind;
    s32 flags;

    out->job_id = unit->job_id;
    out->unit_flags = unit->unit_flags;
    out->active_battle_id = unit->active_battle_id;
    out->character_identity = unit->character_identity;
    out->primary_skillset = unit->primary_skillset;

    flags = unit->initial_team_flags;
    team_kind = (flags & UNIT_TEAM_MASK) != UNIT_TEAM_BLUE;
    if (!(flags & UNIT_TEAM_OR_PLAYER_CONTROL_MASK)) {
        team_kind = 2;
    }

    if ((team_kind == 1) || job_is_special_monster(unit->job_id)) {
        bcopy(unit->career.unlocked_jobs, out->career.unlocked_jobs, sizeof(out->career.unlocked_jobs));
    } else {
        i = job_calculate_unlocked(unit->career.job_levels, unit->unit_flags & UNIT_FLAG_MALE);
        out->career.unlocked_jobs[0] = i >> 16;
        out->career.unlocked_jobs[1] = i >> 8;
        out->career.unlocked_jobs[2] = i;
    }

    bcopy(unit->career.learned_abilities, out->career.learned_abilities, sizeof(out->career.learned_abilities));
    bcopy(unit->career.job_levels, out->career.job_levels, sizeof(out->career.job_levels));
    bcopy(&unit->career.job_points.current, &out->career.job_points.current, sizeof(out->career.job_points.current));
    bcopy(&unit->career.job_points.total, &out->career.job_points.total, sizeof(out->career.job_points.total));

    for (i = 0; i < UNIT_CAREER_JOB_COUNT; i++) {
        if (out->career.job_points.current.values[i] > UNIT_JOB_POINT_MAX) {
            out->career.job_points.current.values[i] = UNIT_JOB_POINT_MAX;
        }
        if (out->career.job_points.total.values[i] > UNIT_JOB_POINT_MAX) {
            out->career.job_points.total.values[i] = UNIT_JOB_POINT_MAX;
        }
    }
}

/* Copy battle unit `unit_id`'s job data into the overlay's single unit
 * record and make it unit 0. */
void status_screen_init_unit_jobs(s32 unit_id) {
    status_screen_copy_unit_jobs(unit_get_stats(unit_id), &g_status_screen_unit_job_data);
    g_status_screen_unit_data[0] = &g_status_screen_unit_job_data;
}

/* The job that heads a unit's job list: a monster's own job, Squire for a
 * generic human identity, and for a special character the identity value
 * itself, which is that character's job id. */
s32 job_list_get_base(s16 unit_id) {
    status_screen_unit_jobs_t* unit = g_status_screen_unit_data[unit_id];
    u32 job_id = unit->character_identity;

    if (job_id == CHARACTER_IDENTITY_MONSTER) {
        job_id = unit->job_id;
    } else if (job_id >= CHARACTER_IDENTITY_GENERIC_MALE) {
        job_id = JOB_ID_SQUIRE;
    }
    return job_id;
}

/*
 * Writes a unit's selectable jobs to a -1-terminated list and returns their
 * count.
 *
 * The base job comes first, followed by the generic jobs whose unlocked bit
 * is set (Bard excluded for female units, Dancer for male units); special
 * monsters and units with the monster identity get only the base job. The
 * third argument is not read.
 */
s32 job_list_build(u8 unit_id, s16* job_ids, s32 unused) {
    u32 unit_index = unit_id;
    status_screen_unit_jobs_t** unit_table = g_status_screen_unit_data;
    s16* out;
    s32 count;
    s32 i;
    s32 job;
    status_screen_unit_jobs_t** slot;
    status_screen_unit_jobs_t** loop_slot;

    (void)unused;

    slot = &unit_table[unit_index];
    bits_init_primary_reader((*slot)->career.unlocked_jobs);
    job_ids[0] = job_list_get_base(unit_id);
    if ((job_is_special_monster((*slot)->job_id) != 0) || ((*slot)->character_identity == CHARACTER_IDENTITY_MONSTER)) {
        job_ids[1] = -1;
        return 1;
    }
    /* Skip Squire's bit: the base job above takes Squire's place. */
    bits_read_primary(1);
    count = 1;
    i = 1;
    loop_slot = slot;
    out = job_ids + 1;
    for (; i < JOB_ID_GENERIC_COUNT; i++) {
        job = i + JOB_ID_SQUIRE;
        if ((bits_read_primary(1) != 0) && ((job != JOB_ID_BARD) || !((*loop_slot)->unit_flags & UNIT_FLAG_FEMALE))
            && ((job != JOB_ID_DANCER) || !((*loop_slot)->unit_flags & UNIT_FLAG_MALE))) {
            *out = job;
            out++;
            count++;
        }
    }
    job_ids[count] = -1;
    return count;
}

/* Return the lowest job ID with this skillset, or -1. */
s32 job_find_first_for_skillset(s32 skillset) {
    s32 i;

    for (i = 0; i < JOB_ID_COUNT; i++) {
        if (job_get_data(i)->skillset == skillset) {
            return i;
        }
    }
    return -1;
}

/* Return the skillset of a job from the job data table. */
s32 job_get_skillset(s32 job_id) {
    return job_get_data(job_id)->skillset;
}

/* Map Squire through Mime to zero-based indexes; unique jobs map to zero. */
s32 job_get_generic_index(s32 job_id) {
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

/* Fill `list` with the abilities of job `job_id`'s skillset in `ability_category` for unit
 * `unit_index`, end it with ABILITY_LIST_ENTRY_END and return the count. A skillset that no
 * job owns falls back to the unit's primary skillset.
 *
 * Abilities the unit has not learned carry ABILITY_LIST_ENTRY_DISABLED, plus
 * ABILITY_LIST_ENTRY_HIDE_DETAILS when JP cannot learn them. A unit whose current job is a
 * special monster counts every ability as learned. A nonzero `unlearned_only` omits the
 * learned abilities, so an empty result means the job is mastered.
 */
s32 ability_list_build(
    s16 unit_index, s16 job_id, menu_ability_category_e ability_category, s16* list, s32 unlearned_only) {
    s16* abilities;
    s16 generic_index;
    s32 count;
    s32 learned;
    s16 ability_id;
    s16 encoded_ability_id;
    s32 byte_offset;
    status_screen_unit_jobs_t* unit_data;
    s32 i; /* holds the skillset id before it counts the two loops */
    s16 lo;
    s16 hi;

    generic_index = job_get_generic_index(job_id);
    i = job_get_skillset(job_id);
    if (job_find_first_for_skillset(i) == -1) {
        i = g_status_screen_unit_data[unit_index]->primary_skillset;
    }
    abilities = (s16*)ability_store_skillset_abilities(i, SKILLSET_ABILITY_FILTER_ALL);

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
    unit_data = g_status_screen_unit_data[unit_index];
    /* The job's learned-ability bits, one per skillset slot, read most significant first.
     * Keeping this offset separate preserves the target's GCC operand order;
     * direct learned_abilities indexing reverses the final commutative addu. */
    byte_offset = generic_index * UNIT_LEARNED_ABILITY_BYTES_PER_JOB
        + offsetof(status_screen_unit_jobs_t, career.learned_abilities);
    bits_init_primary_reader((u8*)(byte_offset + (s32)unit_data));
    for (i = 0; i < SKILLSET_ABILITY_LIST_COUNT; i++) {
        if (job_is_special_monster(g_status_screen_unit_data[unit_index]->job_id) != 0) {
            learned = 1;
        } else {
            learned = bits_read_primary(1);
        }

        ability_id = abilities[i];
        if (ability_id != 0) {
            encoded_ability_id = ability_id;
            if (learned == 0) {
                if ((g_ability_data[ability_id].type_flags >> ABILITY_TYPE_FLAG_CANNOT_LEARN_WITH_JP_SHIFT) == 0) {
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

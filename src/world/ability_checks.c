/* Secondary skillset listing, the Two Swords, Two Hands and Secret Hunt checks, and the ability and equippable RSM
 * lists. */

#include "fft/ability.h"
#include "fft/ability_list.h"
#include "fft/bits.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/job.h"
#include "fft/job_list.h"
#include "fft/skillset.h"
#include "fft/unit.h"
#include "psx/types.h"

/* List the skillsets a formation unit can set as its secondary: those of its unlocked jobs other
 * than its own job and Mime. The list ends with -1; return the count. */
static s32 formation_ability_list_secondary_skillsets(s16 unit_id, s16* skillsets) {
    s16 jobs[24];
    s32 count;
    s32 i;
    s32 job;
    s16 own_job;

    own_job = g_formation_unit_pointers[unit_id]->identity.job_id;
    job_list_build(unit_id, jobs, 1);
    count = 0;
    for (i = 0; jobs[i] != -1; i++) {
        job = jobs[i];
        if (own_job != job && job != JOB_ID_MIME) {
            skillsets[count++] = job_get_skillset(job);
        }
    }
    skillsets[count] = -1;
    return count;
}

/* Return nonzero when the formation unit has the Two Swords support ability set. */
s32 ability_has_two_swords(s16 unit_id) {
    return g_formation_unit_pointers[unit_id]->support_abilities[2] & UNIT_SUPPORT_SET_3_TWO_SWORDS;
}

/* Return nonzero when the formation unit has the Two Hands support ability set. */
s32 formation_ability_has_two_hands(s16 unit_id) {
    return g_formation_unit_pointers[unit_id]->support_abilities[2] & UNIT_SUPPORT_SET_3_TWO_HANDS;
}

/* Return nonzero when the formation unit has the Secret Hunt support ability set. */
s32 formation_ability_has_secret_hunt(s32 unit_id) {
    return g_formation_unit_pointers[(s16)unit_id]->support_abilities[2] & UNIT_SUPPORT_SET_3_SECRET_HUNT;
}

/*
 * List the abilities of `job`'s skillset for a formation unit and return the
 * number listed.
 *
 * `ability_category` selects the ability id range (action, reaction, support,
 * movement, otherwise all). `mode`:
 *   VIEW_ALL          every ability, unlearned ones tagged DISABLED, plus
 *                     HIDE_DETAILS when they cannot be learned with JP;
 *   LEARNED_ONLY      learned ones (for action abilities it lists the unit's
 *                     secondary skillsets instead);
 *   LEARN_MENU        learned ones tagged LEARNED, and unlearned ones that
 *                     can be learned with JP, DISABLED when the unit's JP in
 *                     the job cannot pay for them;
 *   COUNT_UNLEARNED   only counts unlearned ones, with no list written.
 * A special monster counts every ability as learned, and a unit using a
 * monster skillset counts its first three as learned and lists no more.
 *
 * The second loop indexes `abilities` and `list` rather than walking pointers:
 * the target's cursors are the loop optimizer's reduced induction variables,
 * set after the unit-pointer address is formed.
 */
s32 ability_list_build(s16 unit_index, s16 job, menu_ability_category_e ability_category, s16* list, s32 mode) {
    s32 skillset;
    s16 generic;
    s16* abilities;
    s16 lo;
    s16 hi;
    s32 i;
    s32 count;
    s16 jp;
    s32 learned;
    s16 id;

    if (mode == ABILITY_LIST_MODE_LEARNED_ONLY && ability_category == MENU_ABILITY_CATEGORY_ACTION) {
        return formation_ability_list_secondary_skillsets(unit_index, list);
    }
    skillset = job_get_skillset(job);
    generic = job_get_generic_index(job);
    /* A skillset no job owns (a special character's) falls back to the unit's primary skillset. */
    if (job_find_first_for_skillset(skillset) == -1) {
        skillset = g_formation_unit_pointers[unit_index]->abilities.fields.primary_skillset;
    }
    abilities = (s16*)ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ALL);
    if (ability_category == MENU_ABILITY_CATEGORY_ACTION) {
        lo = 1;
        hi = ABILITY_ID_REACTION_FIRST - 1;
    } else if (ability_category == MENU_ABILITY_CATEGORY_REACTION) {
        lo = ABILITY_ID_REACTION_FIRST;
        hi = ABILITY_ID_SUPPORT_FIRST - 1;
    } else if (ability_category == MENU_ABILITY_CATEGORY_SUPPORT) {
        lo = ABILITY_ID_SUPPORT_FIRST;
        hi = ABILITY_ID_MOVEMENT_FIRST - 1;
    } else {
        lo = 1;
        if (ability_category == MENU_ABILITY_CATEGORY_MOVEMENT) {
            lo = ABILITY_ID_MOVEMENT_FIRST;
        }
        hi = ABILITY_ID_RANDOM_FIRST - 1;
    }
    for (i = 0; i < SKILLSET_ABILITY_LIST_COUNT; i++) {
        if (abilities[i] < lo || hi < abilities[i]) {
            abilities[i] = 0;
        }
    }
    count = 0;
    jp = g_formation_unit_pointers[unit_index]->job_points.current.values[generic];
    bits_init_primary_reader(g_formation_unit_pointers[unit_index]->learned_abilities[generic]);
    for (i = 0; i < SKILLSET_ABILITY_LIST_COUNT; i++) {
        if (job_is_special_monster(g_formation_unit_pointers[unit_index]->identity.job_id) != 0) {
            learned = 1;
        } else if (g_formation_unit_pointers[unit_index]->uses_monster_skillset != 0) {
            if (i >= 3) {
                break;
            }
            learned = 1;
        } else {
            learned = bits_read_primary(1);
        }
        id = abilities[i];
        if (id == 0) {
            continue;
        }
        if (mode == ABILITY_LIST_MODE_VIEW_ALL) {
            if (!learned) {
                if (!(g_ability_data[id].type_flags >> ABILITY_TYPE_FLAG_CANNOT_LEARN_WITH_JP_SHIFT)) {
                    id |= ABILITY_LIST_ENTRY_DISABLED;
                } else {
                    id |= ABILITY_LIST_ENTRY_DISABLED | ABILITY_LIST_ENTRY_HIDE_DETAILS;
                }
            }
            list[count] = id;
            count++;
        } else if (mode == ABILITY_LIST_MODE_LEARNED_ONLY) {
            if (learned) {
                list[count] = id;
                count++;
            }
        } else if (mode == ABILITY_LIST_MODE_LEARN_MENU) {
            if (!learned) {
                if (g_ability_data[id].type_flags >> ABILITY_TYPE_FLAG_CANNOT_LEARN_WITH_JP_SHIFT) {
                    continue;
                }
                if (jp < g_ability_data[id].jp_cost) {
                    id |= ABILITY_LIST_ENTRY_DISABLED;
                }
            } else {
                id |= ABILITY_LIST_ENTRY_LEARNED;
            }
            list[count] = id;
            count++;
        } else if (mode == ABILITY_LIST_MODE_COUNT_UNLEARNED) {
            if (!learned) {
                count++;
            }
        }
    }
    if (mode != ABILITY_LIST_MODE_COUNT_UNLEARNED) {
        list[count] = -1;
    }
    return count;
}

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
s32 formation_ability_build_rsm_list(s16 unit_index, s32 slot, s16* out) {
    s16 abilities[204];
    s32 count;
    s32 i;
    s32 n;
    s32 swap;
    s32 last;
    s16 job_id = g_formation_unit_pointers[unit_index]->identity.job_id;

    count = ability_list_build(
        unit_index, job_list_get_base(unit_index), slot, abilities, ABILITY_LIST_MODE_LEARNED_ONLY);
    for (i = 1; i < JOB_ID_GENERIC_COUNT - 1; i++) {
        count += ability_list_build(
            unit_index, JOB_ID_SQUIRE + i, slot, &abilities[count], ABILITY_LIST_MODE_LEARNED_ONLY);
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

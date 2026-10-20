/* Generate equipment and learned abilities for an ENTD unit. The rolls use its level,
 * unlocked jobs and permitted skillsets before final stats are built. */

#include "fft/ability.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/random.h"
#include "fft/skillset.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "fft/unit_identity.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Copy count bytes one at a time; the source comes first, unlike memcpy. */
void mem_copy_bytes_source_first(const void* source, void* destination, int count) {
    const u8* src = source;
    u8* dst = destination;
    int i;

    for (i = 0; i < count; i++) {
        *dst++ = *src++;
    }
}

/* Pick a random non-rare item for one equipment slot (item_type_flag) that the unit can equip at
 * its level, optionally limited to one item type and to weapons with a flag in weapon_flag_mask.
 * Returns ENTD_VALUE_RANDOM when nothing fits. The candidate list restarts only when an item with
 * a higher required level appears, so items scanned after it with a lower level still join it. */
s32 unit_random_equipment_generate(
    const unit_stats_t* unit, u8 item_type_flag, u8 weapon_flag_mask, u8 required_item_type) {
    u8 candidates[128];
    s32 first;
    s32 last;
    s32 i;
    u8 count;
    u8 best;
    u32 level;
    u8 value; /* item byte / candidate index scratch */
    u32 type;
    item_data_t* item;

    switch (item_type_flag) {
    case ITEM_TYPE_FLAG_WEAPON:
        first = ITEM_ID_DAGGER;
        last = ITEM_ID_SHIELD_FIRST;
        break;
    case ITEM_TYPE_FLAG_SHIELD:
        first = ITEM_ID_SHIELD_FIRST;
        last = ITEM_ID_HEADGEAR_FIRST;
        break;
    case ITEM_TYPE_FLAG_HEADGEAR:
        first = ITEM_ID_HEADGEAR_FIRST;
        last = ITEM_ID_BODY_ARMOR_FIRST;
        break;
    case ITEM_TYPE_FLAG_BODY_ARMOR:
        first = ITEM_ID_BODY_ARMOR_FIRST;
        last = ITEM_ID_ACCESSORY_FIRST;
        break;
    case ITEM_TYPE_FLAG_ACCESSORY:
        first = ITEM_ID_ACCESSORY_FIRST;
        last = ITEM_ID_CONSUMABLE_FIRST;
        break;
    default:
        first = ITEM_ID_NOTHING;
        last = ITEM_ID_COUNT;
        break;
    }

    count = 0;
    level = unit->level;
    best = 0;
    for (i = first; i < last; i++) {
        item = &g_item_primary_data[i];
        value = item->type_flags;
        if (value & ITEM_TYPE_FLAG_RARE) {
            continue;
        }
        value = item->type;
        if (required_item_type != ITEM_TYPE_FILTER_ANY && value != required_item_type) {
            continue;
        }
        type = value;
        if ((unit->equipment_categories[EQUIPPABLE_ITEM_TYPE_BYTE_INDEX(type)] & EQUIPPABLE_ITEM_TYPE_BYTE_MASK(type))
            == 0) {
            continue;
        }
        if (weapon_flag_mask != 0) {
            if ((weapon_flag_mask & g_item_weapon_data[item->secondary_data_id].flags) == 0) {
                continue;
            }
        }
        value = item->required_level;
        if (level < value) {
            continue;
        }
        if (best < value) {
            best = value;
            count = 0;
        }
        candidates[count++] = i;
    }

    if (count == 0) {
        return ENTD_VALUE_RANDOM;
    }
    value = (rand() * count) / 0x8000;
    return candidates[value];
}

/* Learns abilities for one job of a unit.
 * job_id 0x4a (Squire) stands for the unit's base job.  With an ENTD primary
 * skillset of 0 every ability of the unit's own job is learned; otherwise
 * each ability is rolled against its learn chance and bought with job JP.
 * Two scratch locals preserve the native register reuse: `i` doubles as the
 * unlocked-job mask and `scratch` holds both those bits and the learn chance;
 * separate locals change the register allocation. */
void unit_job_abilities_learn(unit_stats_t* unit, s32 job_id, const entd_unit_t* entd) {
    s32 job_index = job_id - JOB_ID_SQUIRE;
    s32 skillset;
    s32 auto_learn = 0;
    s32 learned;
    s32 i;
    u16 ability_id;
    s32 scratch;
    ability_data_t* ability;
    u8 character;
    s32 unit_flags;

    if (job_id == JOB_ID_SQUIRE) {
        character = unit->character_identity;
        if (character < CHARACTER_IDENTITY_SELECTOR_FIRST
            || character >= CHARACTER_IDENTITY_SELECTOR_FIRST + CHARACTER_IDENTITY_GENERIC_HUMAN_COUNT) {
            if (character == CHARACTER_IDENTITY_MONSTER) {
                job_id = unit->job_id;
            } else {
                job_id = unit->character_identity;
            }
        }
    } else {
        i = UNIT_UNLOCKED_JOB_SQUIRE >> job_index;
        scratch = (unit->career.unlocked_jobs[0] << 16) + (unit->career.unlocked_jobs[1] << 8)
            + unit->career.unlocked_jobs[2];
        if ((scratch & i) == 0) {
            return;
        }
    }

    unit_flags = unit->unit_flags;
    if (job_id == JOB_ID_BARD && (unit_flags & UNIT_FLAG_FEMALE)) {
        return;
    }
    if (job_id == JOB_ID_DANCER && (unit_flags & UNIT_FLAG_MALE)) {
        return;
    }

    if (job_id == unit->job_id) {
        skillset = unit->primary_skillset;
        if (entd->primary_skillset == 0) {
            auto_learn = 1;
        }
    } else {
        skillset = g_job_data_pointer[job_id].skillset;
    }
    for (i = 0; i < SKILLSET_ABILITY_LIST_COUNT; i++) {
        learned = 0;
        ability_id = ability_skillset_id_get(skillset, i);
        if (ability_id != 0) {
            ability = &g_ability_data[ability_id];
            scratch = ability->learn_rate;
            if (auto_learn != 0) {
                learned = 1;
            } else if (random_roll_pass_fail(100, scratch) == 0) {
                u16 cost = ability->jp_cost;
                if (unit->career.job_points.current.values[job_index] >= cost) {
                    unit->career.job_points.current.values[job_index] -= cost;
                    unit->career.learned_abilities[job_index][i / 8] |= 0x80 >> (i % 8);
                    learned = 1;
                }
            }
            if (learned != 0) {
                unit->career.learned_abilities[job_index][i / 8] |= 0x80 >> (i % 8);
            }
        }
    }
}

#define RSM_CANDIDATE_COUNT 480

/* Picks a random reaction/support/movement ability (selected by `flags` from
 * rsm_ability_filter_e) from the abilities the unit has learned
 * across its unlocked jobs, or returns `ability_id` when it is not random. */
u16 unit_rsm_select(const unit_stats_t* unit, u16 ability_id, s32 flags, const entd_unit_t* entd) {
    u16 candidates[RSM_CANDIDATE_COUNT];
    u16 innate[4];
    u8 known;
    s32 count;
    s32 i;
    s32 secondary_unchecked;
    s32 job;
    s32 j;
    s32 keep;
    u16 rel;
    u32 id;
    u16 checked_id;
    s32 checking;
    u8 skillset;
    u8 secondary;
    u8 job_id;
    s32 sprite;

    if (ability_id < ABILITY_ID_RANDOM_FIRST) {
        return ability_id;
    }
    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        innate[i] = unit->innate_abilities[i];
    }
    count = 0;
    secondary_unchecked = 1;
    secondary = unit->secondary_skillset;
    for (i = 0; i < UNIT_CAREER_JOB_COUNT; i++) {
        skillset = 0;
        checking = 1;
        if (i == UNIT_CAREER_JOB_INDEX_MIME) {
            /* Mime slot: use the secondary skillset when no job provided it. */
            if (secondary_unchecked == 0) {
                continue;
            }
            skillset = secondary;
            if (skillset == 0) {
                continue;
            }
            checking = 0;
        } else {
            job = i + JOB_ID_SQUIRE;
            if (i == 0) {
                sprite = unit->character_identity;
                if ((u8)(sprite - CHARACTER_IDENTITY_SELECTOR_FIRST) >= CHARACTER_IDENTITY_GENERIC_HUMAN_COUNT) {
                    job = sprite;
                }
                if (entd->primary_skillset != 0 && entd->primary_skillset != SKILLSET_ID_NONE) {
                    skillset = entd->primary_skillset;
                }
            }
        }
        if (skillset == 0) {
            job_id = job;
            if (job_id == 0) {
                continue;
            }
            skillset = g_job_data_pointer[job_id].skillset;
        }
        if (skillset == secondary) {
            secondary_unchecked = 0;
        }
        if (checking) {
            known = unit->career.learned_abilities[i][2];
        }
        for (j = HUMAN_SKILLSET_ACTION_SLOT_COUNT; j < HUMAN_SKILLSET_ABILITY_COUNT; j++) {
            if (checking) {
                if (!(known & (0x80 >> (j % 8)))) {
                    continue;
                }
            }
            keep = 0;
            id = ability_skillset_id_get(skillset, j);
            checked_id = id;
            if (checked_id == ABILITY_ID_REACTION_SUNKEN_STATE)
                continue;
            if (checked_id == innate[0])
                continue;
            if (checked_id == innate[1])
                continue;
            if (checked_id == innate[2])
                continue;
            if (checked_id == innate[3])
                continue;
            if (flags & RSM_ABILITY_FILTER_REACTION) {
                rel = id - ABILITY_ID_REACTION_FIRST;
                keep = rel < ABILITY_ID_SUPPORT_FIRST - ABILITY_ID_REACTION_FIRST;
            }
            if (flags & RSM_ABILITY_FILTER_SUPPORT) {
                rel = id - ABILITY_ID_SUPPORT_FIRST;
                if (rel < ABILITY_ID_MOVEMENT_FIRST - ABILITY_ID_SUPPORT_FIRST) {
                    keep = 1;
                }
            }
            if (flags & RSM_ABILITY_FILTER_MOVEMENT) {
                u16 movement_id = id;
                if (movement_id >= ABILITY_ID_MOVEMENT_FIRST) {
                    keep = 1;
                }
            }
            if (keep) {
                candidates[count++] = id;
            }
        }
    }
    if (count == 0) {
        return 0;
    }
    return candidates[(rand() * count) / 0x8000];
}

/* Return the first job id below JOB_ID_COUNT - 1 whose job record has this
 * skillset. The result is 0 when none has, which is also job 0's id. Nothing
 * in the tree calls it. */
s32 job_skillset_find(u8 skillset_id) {
    u8 job_id = 0;
    s32 result;

    for (; (result = job_id < JOB_ID_COUNT - 1) != 0; job_id++) {
        if (g_job_data_pointer[job_id].skillset == skillset_id) {
            return job_id;
        }
    }
    return result;
}

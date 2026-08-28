#include "fft/character_identity.h"
#include "fft/job.h"
#include "fft/main_runtime.h"
#include "fft/main_unit.h"
#include "psx/types.h"

#define RSM_CANDIDATE_COUNT     480
#define RSM_SKILLSET_SLOT_FIRST 0x10

/* Picks a random reaction/support/movement ability (selected by `flags` from
 * rsm_ability_filter_e) from the abilities the unit has learned
 * across its unlocked jobs, or returns `ability_id` when it is not random. */
u16 main_unit_calculate_rsm(battle_stats_t* unit, u16 ability_id, s32 flags, entd_unit_t* entd) {
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
    s32 id;
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
    i = 0;
    do {
        innate[i] = unit->innate_abilities[i];
        i++;
    } while (i < 4);
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
            known = unit->learned_abilities[i * UNIT_LEARNED_ABILITY_BYTES_PER_JOB + 2];
        }
        for (j = RSM_SKILLSET_SLOT_FIRST; j < HUMAN_SKILLSET_ABILITY_COUNT; j++) {
            if (checking) {
                if (!(known & (0x80 >> (j % 8)))) {
                    continue;
                }
            }
            keep = 0;
            id = main_ability_get_id_from_skillset(skillset, j);
            if ((id & 0xFFFF) == ABILITY_ID_REACTION_SUNKEN_STATE)
                continue;
            if ((id & 0xFFFF) == innate[0])
                continue;
            if ((id & 0xFFFF) == innate[1])
                continue;
            if ((id & 0xFFFF) == innate[2])
                continue;
            if ((id & 0xFFFF) == innate[3])
                continue;
            if (flags & RSM_ABILITY_FILTER_REACTION) {
                rel = id - ABILITY_ID_REACTION_FIRST;
                keep = rel < 0x20;
            }
            if (flags & RSM_ABILITY_FILTER_SUPPORT) {
                rel = id - ABILITY_ID_SUPPORT_FIRST;
                if (rel < 0x20) {
                    keep = 1;
                }
            }
            if (flags & RSM_ABILITY_FILTER_MOVEMENT) {
                if ((u32)(id & 0xFFFF) >= ABILITY_ID_MOVEMENT_FIRST) {
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

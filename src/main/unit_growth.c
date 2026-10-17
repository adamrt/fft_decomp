/* The source-first byte copy, random equipment, job ability learning, RSM calculation, status init and CT, stat
 * refresh, level up and growth, job unlocks and levels, ability range and AOE, palettes, spritesheets and action data
 * copies. */

#include "fft/ability.h"
#include "fft/action.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/party.h"
#include "fft/skillset.h"
#include "fft/status.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "psx/libc.h"
#include "psx/types.h"

static void status_init_ct(unit_stats_t* unit);
static void status_update_unit_flags_and_ct(unit_stats_t* unit);
static u8 unit_get_spritesheet_palette(const unit_stats_t* unit, u8* palette);
static void unit_update_stats_statuses_and_equipment(unit_stats_t* unit, s32 skip_status_check, s32 initializing);

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
s32 unit_calculate_random_equipment(
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
 * The original reuses two scratch variables: `i` doubles as the unlocked-job
 * mask and `scratch` holds both the unlocked-job bits and the learn chance;
 * separate locals change the register allocation. */
void unit_learn_job_abilities(unit_stats_t* unit, s32 job_id, const entd_unit_t* entd) {
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
        ability_id = ability_get_id_from_skillset(skillset, i);
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
u16 unit_calculate_rsm(const unit_stats_t* unit, u16 ability_id, s32 flags, const entd_unit_t* entd) {
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
            id = ability_get_id_from_skillset(skillset, j);
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
s32 job_find_by_skillset(u8 skillset_id) {
    u8 job_id = 0;
    s32 result;

    for (; (result = job_id < JOB_ID_COUNT - 1) != 0; job_id++) {
        if (g_job_data_pointer[job_id].skillset == skillset_id) {
            return job_id;
        }
    }
    return result;
}

/* Seed inflicted_status with the current statuses the unit does not have
 * innately, then set up the status CTs (status_init_ct) and reconcile the
 * status flags. */
static void status_init_unit(unit_stats_t* unit) {
    s32 status_set;

    for (status_set = 0; status_set < STATUS_BYTE_COUNT; status_set++) {
        unit->inflicted_status[status_set]
            = unit->status_sets.current[status_set] & ~unit->status_sets.innate[status_set];
    }
    status_init_ct(unit);
    status_update_unit_flags_and_ct(unit);
}

/* Clear the timed-status CTs and start the death counter at 3, or at 0xff for
 * an immortal unit and for one with a save- or load-formation flag. */
static void status_init_ct(unit_stats_t* unit) {
    mem_zero(unit->status_ct, sizeof(unit->status_ct));
    if ((unit->team_flags & UNIT_TEAM_FLAG_IMMORTAL)
        || (unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION))) {
        unit->death_counter = 0xff;
    } else {
        unit->death_counter = 3;
    }
}

/* Apply Float and reconcile the unit's status flags and status CT. */
static void status_update_unit_flags_and_ct(unit_stats_t* unit) {
    s32 status;
    u8 immunity;

    if (unit->ability_flags.movement[2] & UNIT_MOVEMENT_SET_3_FLOAT) {
        unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] |= STATUS_BYTE_MASK(STATUS_ID_FLOAT);
    }

    for (status = 0; status < STATUS_BYTE_COUNT; status++) {
        immunity = unit->status_sets.immunity[status];
        immunity &= ~unit->status_sets.innate[status];
        unit->status_sets.immunity[status] = immunity;
        unit->status_sets.current[status] = unit->status_sets.innate[status] | unit->inflicted_status[status];
    }

    for (status = STATUS_ID_POISON; status < STATUS_COUNT; status++) {
        s32 status_byte = status / 8;
        s32 status_bit = status & 7;

        if ((unit->inflicted_status[status_byte] & (0x80 >> status_bit))
            && unit->status_ct[STATUS_CT_INDEX(status)] == 0) {
            status_set_ct(unit, status, 0);
        }
    }
}

/* unit_update_stats_statuses_and_equipment with skip_status_check set: stats
 * are re-derived without the critical and status notifications. */
void unit_update_formation_stats(unit_stats_t* unit) {
    unit_update_stats_statuses_and_equipment(unit, 1, 0);
}

/* unit_update_stats_statuses_and_equipment with both flags clear: stats are
 * re-derived and the status notifications run. */
void unit_refresh_stats_and_statuses(unit_stats_t* unit) {
    unit_update_stats_statuses_and_equipment(unit, 0, 0);
}

/* unit_update_stats_statuses_and_equipment in its initializing form: the
 * inflicted statuses are seeded from the current ones and their CTs cleared.
 * Nothing in the tree calls it. */
void unit_init_stats_and_statuses(unit_stats_t* unit) {
    unit_update_stats_statuses_and_equipment(unit, 0, 1);
}

/* Re-derives the job-dependent stats (move/jump, equippable categories,
 * innate/immune/starting statuses, elemental affinity, R/S/M flags and
 * equipment bonuses), then reconciles the status bits.  `skip_status_check`
 * leaves the critical/status notifications alone; `initializing` seeds the
 * inflicted statuses from the current ones and clears their CTs. */
static void unit_update_stats_statuses_and_equipment(unit_stats_t* unit, s32 skip_status_check, s32 initializing) {
    u8 saved_inflicted[STATUS_BYTE_COUNT];
    u8 saved_current[STATUS_BYTE_COUNT];
    s32 i;
    s32 status_id;
    s32 mask;
    s32 byte_index;
    s32 current_bit;
    s32 battle_id = unit->battle_id;
    s32 saved_bit;
    u8 formation_index;
    job_data_t* job;

    status_store_current(unit);
    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        saved_current[i] = unit->status_sets.current[i];
        saved_inflicted[i] = unit->inflicted_status[i];
    }
    job = &g_job_data_pointer[unit->job_id];
    unit->move = job->move;
    unit->jump = job->jump & JOB_JUMP_VALUE_MASK;
    mem_copy_bytes_source_first(
        job->equipment_categories, unit->equipment_categories, sizeof(job->equipment_categories));
    mem_copy_bytes_source_first(&job->status_sets, &unit->status_sets, sizeof(job->status_sets));
    mem_copy_bytes_source_first(job->elemental_affinity, unit->elemental_affinity, sizeof(job->elemental_affinity));
    unit->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN] = 0;
    unit_enable_rsm_flags(unit);
    unit_calculate_move_jump(unit, 1);
    unit_set_equipment_stats(unit);
    unit_set_equipment_attributes(unit, 0);
    unit_set_equippable_items(unit);

    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        if (initializing) {
            unit->inflicted_status[i] = unit->status_sets.current[i];
        } else {
            unit->inflicted_status[i] = saved_inflicted[i];
        }
    }

    formation_index = unit->formation_index;
    unit->formation_index = UNIT_FORMATION_INDEX_NONE;
    status_update_unit_flags_and_ct(unit);
    unit->formation_index = formation_index;
    status_store_current(unit);

    if (skip_status_check != 0) {
        return;
    }
    if ((unit->max_hp / 5) >= unit->hp) {
        unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)] |= STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
    } else {
        unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)] &= ~STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
    }
    status_store_current(unit);

    for (i = 0, status_id = STATUS_HANDLER_INDEX(STATUS_ID_UNNAMED_00); i < STATUS_COUNT; i++, status_id++) {
        byte_index = i / 8;
        mask = 0x80 >> (i & 7);
        saved_bit = saved_current[byte_index] & mask;
        current_bit = unit->status_sets.current[byte_index] & mask;
        if (initializing != 0 || saved_bit != current_bit) {
            if (current_bit != 0) {
                if (initializing != 0) {
                    status_set_ct(unit, i, 0);
                }
                status_queue_display_flag_change_if_executing(status_id, 1, battle_id);
            } else {
                status_queue_display_flag_change_if_executing(status_id, 0, battle_id);
            }
        }
    }
}

/* Level up at 100 EXP: one level of growth, and EXP goes back to 0 (the excess
 * is lost). At level 99 EXP is held at 99 instead. Returns 1 when the level
 * rose. */
s32 unit_check_level_up(unit_stats_t* unit) {
    u8 level;

    if (unit->experience < UNIT_LEVEL_UP_EXPERIENCE) {
        return 0;
    }

    level = unit->level;
    if (level >= UNIT_LEVEL_MAX) {
        unit->experience = UNIT_LEVEL_EXPERIENCE_MAX;
        return 0;
    }

    unit_apply_level_growth(unit, 0);
    unit->experience = 0;
    unit->level = level + 1;
    return 1;
}

/* Raises a roster unit by `level_delta` levels (capped at 99), growing each
 * 24-bit raw stat by raw / (level + growth) per level, and returns the new
 * level.  The growth table is the job's stat_growths pairs. */
u32 party_level_unit_to_target(party_unit_t* party_unit, s32 level_delta) {
    u32 current_level = party_unit->level;
    u32 level = current_level + level_delta;
    job_stat_growth_t* growth_table;
    unit_raw_stat_t* raw_stats;
    s32 levels_to_gain;
    s32 level_index;
    s32 i;
    u32 raw;
    u32 divisor;

    raw_stats = party_unit->raw_stats;
    growth_table = g_job_data_pointer[party_unit->job_id].stat_growths;
    if (level > UNIT_LEVEL_MAX) {
        level = UNIT_LEVEL_MAX;
    }
    levels_to_gain = level - current_level;
    if (levels_to_gain <= 0) {
        return level;
    }
    for (level_index = 0; level_index < levels_to_gain; level_index++) {
        for (i = 0; i < UNIT_RAW_STAT_COUNT; i++) {
            level = party_unit->level;
            raw = raw_stats[i].low + (raw_stats[i].middle << 8) + (raw_stats[i].high << 16);
            if (growth_table[i].growth == 0) {
                divisor = level + 1;
            } else {
                divisor = level + growth_table[i].growth;
            }
            raw += raw / divisor;
            if (raw > 0xffffff) {
                raw = 0xffffff;
            }
            raw_stats[i].low = raw;
            raw_stats[i].middle = raw >> 8;
            raw_stats[i].high = raw >> 16;
        }
        party_unit->level++;
    }
    return party_unit->level;
}

/* Applies one level of growth (or its inverse for a level down) to each
 * 24-bit raw stat: raw +/- raw / (growth + level), a zero growth counting as
 * 1, capped at 0xffffff; then refreshes the derived stats and clamps HP/MP. */
void unit_apply_level_growth(unit_stats_t* unit, s32 level_down) {
    unit_raw_stat_t* raw_stats = unit->raw_stats;
    job_stat_growth_t* growth = unit->stat_growths;
    u32 value;
    u32 divisor;
    s32 level = unit->level;
    u32 minimum_growth;
    s32 i;

    for (i = 0; i < UNIT_RAW_STAT_COUNT; i++) {
        divisor = growth[i].growth;
        value = raw_stats[i].low + (raw_stats[i].middle << 8) + (raw_stats[i].high << 16);
        minimum_growth = divisor;
        if (divisor == 0) {
            minimum_growth = 1;
        }
        divisor = minimum_growth + level;
        /* Drawn and discarded: each stat's growth consumes one random number. */
        rand();
        if (level_down) {
            value -= value / divisor;
        } else {
            value += value / divisor;
        }
        if (value > 0x00ffffff) {
            value = 0x00ffffff;
        }
        raw_stats[i].low = value;
        raw_stats[i].middle = value >> 8;
        raw_stats[i].high = value >> 16;
    }

    unit_refresh_stats_and_statuses(unit);
    if (unit->hp > unit->max_hp) {
        unit->hp = unit->max_hp;
    }
    if (unit->mp > unit->max_mp) {
        unit->mp = unit->max_mp;
    }
}

/* Start a timed status's CT from g_status_effect_data, or clear it when
 * removing. For Dead, in either case, the death counter is set to 3, or to
 * 0xff for an immortal unit and for one with a save- or load-formation flag.
 * Returns -1 when Death Sentence is already counting, so a second one does not
 * restart it; otherwise 0. */
s32 status_set_ct(unit_stats_t* unit, s32 status_id, s32 removing) {
    u32 ct_index;
    s32 stored_index;

    if (status_id == STATUS_ID_DEAD) {
        if ((unit->team_flags & UNIT_TEAM_FLAG_IMMORTAL)
            || (unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION))) {
            unit->death_counter = 0xff;
        } else {
            unit->death_counter = 3;
        }
    }

    ct_index = STATUS_CT_INDEX(status_id);
    if (ct_index >= STATUS_TIMED_COUNT) {
        return 0;
    }
    stored_index = ct_index;
    if (removing) {
        unit->status_ct[stored_index] = 0;
        return 0;
    }
    if ((stored_index == STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE))
        && (unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] != 0)) {
        return -1;
    }
    unit->status_ct[stored_index] = g_status_effect_data[status_id].ct;
    return 0;
}

/* Compute a unit's unlocked-job bitset (Squire in bit 23, always set) from its packed job levels:
 * a job unlocks when every level nibble meets its g_job_unlock_requirements row. The other
 * sex's Bard or Dancer level counts as maxed for the check, then that job is masked out; a monster
 * unlocks nothing. */
u32 job_calculate_unlocked(const u8* packed_job_levels, u32 gender_flags) {
    u8 levels[UNIT_CAREER_JOB_COUNT];
    s32 unlock_bit;
    s32 unlocked;
    s32 job;
    s32 i;
    s32 j;
    s32 blocked;

    for (i = 0; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
        levels[i] = packed_job_levels[i];
    }
    if (gender_flags & UNIT_FLAG_FEMALE) {
        levels[UNIT_JOB_LEVEL_BYTE_INDEX_CALCULATOR_BARD] |= UNIT_JOB_LEVEL_LOW_NIBBLE_MASK;
    }
    if (gender_flags & UNIT_FLAG_MALE) {
        levels[UNIT_JOB_LEVEL_BYTE_INDEX_DANCER_MIME] |= UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK;
    }
    if (gender_flags & UNIT_FLAG_MONSTER) {
        return 0;
    }

    unlock_bit = UNIT_UNLOCKED_JOB_SQUIRE;
    unlocked = UNIT_UNLOCKED_JOB_SQUIRE;
    for (job = 0; job < UNIT_CAREER_JOB_COUNT - 1; job++) {
        unlock_bit /= 2;
        blocked = 0;
        for (j = 0; j < UNIT_JOB_LEVEL_BYTE_COUNT; j++) {
            if ((g_job_unlock_requirements[job][j] & UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK)
                > (levels[j] & UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK)) {
                blocked = 1;
                break;
            }
            if ((g_job_unlock_requirements[job][j] & UNIT_JOB_LEVEL_LOW_NIBBLE_MASK)
                > (levels[j] & UNIT_JOB_LEVEL_LOW_NIBBLE_MASK)) {
                blocked = 1;
                break;
            }
        }
        if (!blocked) {
            unlocked |= unlock_bit;
        }
    }

    if (gender_flags & UNIT_FLAG_MALE) {
        unlocked &= UNIT_UNLOCKED_JOB_MASK & ~UNIT_UNLOCKED_JOB_DANCER;
    }
    if (gender_flags & UNIT_FLAG_FEMALE) {
        unlocked &= UNIT_UNLOCKED_JOB_MASK & ~UNIT_UNLOCKED_JOB_BARD;
    }
    return unlocked;
}

/* Awards proposition JP to a roster unit's current generic job (special jobs
 * use slot 0), recomputes the job level nibble and the unlocked-job bitset.
 * Returns -1 when the unit or JP amount is invalid. */
s32 job_add_proposition_jp(u32 party_index, s32 jp) {
    s32 slot;
    party_unit_t* party;
    u8 job;
    s32 total;
    s32 level;
    s32 levels;
    u8 level_byte;
    u16 total_jp;

    if (jp < 0) {
        return -1;
    }
    if (party_index >= PARTY_ROSTER_SLOT_COUNT) {
        return -1;
    }
    party = party_get_unit(party_index);
    if (party->party_id == PARTY_ID_NONE) {
        return -1;
    }
    if (party->gender_flags & UNIT_FLAG_MONSTER) {
        return -1;
    }
    job = party->job_id;
    if (job >= JOB_ID_GENERIC_FIRST && job <= JOB_ID_GENERIC_LAST) {
        slot = job - JOB_ID_GENERIC_FIRST;
    } else {
        slot = 0;
    }

    /* Each serialized JP entry is a little-endian byte pair. */
    total = party->career.job_points.current.entries[slot].low
        + (party->career.job_points.current.entries[slot].high << 8) + jp;
    if (total > UNIT_JOB_POINT_MAX) {
        total = UNIT_JOB_POINT_MAX;
    }
    party->career.job_points.current.entries[slot].low = total;
    party->career.job_points.current.entries[slot].high = total / 256;

    total = party->career.job_points.total.entries[slot].low + (party->career.job_points.total.entries[slot].high << 8)
        + jp;
    if (total > UNIT_JOB_POINT_MAX) {
        total = UNIT_JOB_POINT_MAX;
    }
    party->career.job_points.total.entries[slot].low = total;
    party->career.job_points.total.entries[slot].high = total / 256;
    total_jp = total;
    level = job_calculate_level(total_jp);

    levels = party->career.job_levels[slot / 2];
    level_byte = level;
    if ((slot & 1) != 0) {
        levels = level + (levels & 0xF0);
    } else {
        levels = (levels & 0xF) | (level_byte << 4);
    }
    party->career.job_levels[slot / 2] = levels;

    job_store_unlock_bitset(
        party->career.unlocked_jobs, job_calculate_unlocked(party->career.job_levels, party->gender_flags));
    return 0;
}

/* Store the low 24 bits of unlocked_jobs as three bytes, most significant first. */
void job_store_unlock_bitset(u8 destination[UNIT_UNLOCKED_JOB_BYTE_COUNT], u32 unlocked_jobs) {
    *destination++ = unlocked_jobs >> 16;
    *destination++ = unlocked_jobs >> 8;
    *destination = unlocked_jobs;
}

/* Job level: how many of the eight thresholds in
 * g_job_level_jp_requirements the given JP has reached (0..8). */
u32 job_calculate_level(u16 job_jp) {
    s32 job_level = 0;
    s32 level;

    for (level = 0; level < UNIT_JOB_LEVEL_MAX; level++) {
        if (job_jp >= g_job_level_jp_requirements[level]) {
            job_level++;
        }
    }
    return job_level;
}

/* Pack each pair of job levels from the jobs' total JP: the first job of a pair in the high
 * nibble, the second in the low. */
void unit_init_job_levels(const u16* unit_job_jp, u8* unit_job_levels) {
    s32 i;
    s32 level;

    for (i = 0; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
        level = job_calculate_level(unit_job_jp[i * UNIT_JOBS_PER_LEVEL_BYTE]) << 4;
        unit_job_levels[i] = level | job_calculate_level(unit_job_jp[i * UNIT_JOBS_PER_LEVEL_BYTE + 1]);
    }
}

/* Clear the per-battle unit state: manual auto-battle with no target, CT 0, no active turn or
 * ability outcome, no charging ability (0xff), not mounted and no KOs. */
void unit_reset_battle_state(unit_stats_t* unit) {
    unit->auto_battle_setting = AI_AUTOBATTLE_MANUAL;
    unit->auto_battle_target = 0;
    unit->ct = 0;
    unit->has_turn = 0;
    unit->ability_outcome = 0;
    unit->charged_ability_ct = 0xff;
    unit->mount_info = 0;
    unit->ko_count = 0;
}

/* Raise a Speed of 0 to 1, optionally clear the war trophy and bonus money, and
 * initialize the unit's status data (status_init_unit). */
void unit_init_status_and_rewards(unit_stats_t* unit, s32 clear_rewards) {
    if (unit->attributes[UNIT_ATTRIBUTE_SPEED] == 0) {
        unit->attributes[UNIT_ATTRIBUTE_SPEED] = 1;
    }
    if (clear_rewards != 0) {
        unit->war_trophy = ITEM_ID_NOTHING;
        unit->bonus_money_modifier = 0;
    }
    status_init_unit(unit);
}

/* The range byte of an ordinary ability's range record; 0 for an id from
 * ABILITY_ID_ITEM_FIRST up, which has no such record. Nothing in the tree
 * calls it. */
u32 ability_get_range(u32 ability_id) {
    u16 index = ability_id;

    return index < ABILITY_ID_ITEM_FIRST ? g_ability_range_data[index].range : 0;
}

/* The area-of-effect byte of an ordinary ability's range record; 0 for an id from
 * ABILITY_ID_ITEM_FIRST up, which has no such record. Nothing in the tree calls
 * it. */
u32 ability_get_aoe(u32 ability_id) {
    u16 index = ability_id;

    return index < ABILITY_ID_ITEM_FIRST ? g_ability_range_data[index].aoe : 0;
}

/* Resolve the unit's spritesheet and palette (unit_get_spritesheet_palette), store both in
 * the unit and return the spritesheet id. */
u8 unit_calculate_palette_spritesheet(unit_stats_t* unit, u8* palette) {
    u8 spritesheet = unit_get_spritesheet_palette(unit, palette);

    unit->spritesheet_id = spritesheet;
    unit->job_portrait_palette = *palette;
    return spritesheet;
}

/* Roll 0..maximum-1 and compare it with threshold: returns 0 when the roll is
 * below threshold and 1 otherwise. An event tested with `== 0` therefore has
 * threshold-in-maximum odds. */
s32 random_roll_pass_fail(s32 maximum, s32 threshold) {
    s32 roll = rand() * maximum / 0x8000;

    return roll >= threshold;
}

/* Resolve a unit's character identity to a spritesheet id; *palette is 0 except
 * for a monster.
 *
 * An identity below 0x80 is its own spritesheet. The generic male and female
 * selectors give the job's sheet, 0x60 + (job - JOB_ID_SQUIRE) * 2 plus one for a
 * female, with Dancer and Mime counted one job lower. The monster selector
 * returns the spritesheet and palette already in the unit (from its job record).
 * Any higher identity gives job_id + 0x28. */
static u8 unit_get_spritesheet_palette(const unit_stats_t* unit, u8* palette) {
    u8 job_id;
    u8 sprite_set;

    *palette = 0;
    job_id = unit->job_id;
    sprite_set = unit->character_identity;
    if ((job_id == JOB_ID_DANCER) || (job_id == JOB_ID_MIME)) {
        job_id--;
    }
    if (sprite_set < CHARACTER_IDENTITY_SELECTOR_FIRST) {
        return sprite_set;
    }
    if (sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
        sprite_set = job_id * 2 - (JOB_ID_SQUIRE * 2 - CHARACTER_IDENTITY_FIXED_MALE_SQUIRE);
        return sprite_set;
    }
    if (sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
        sprite_set = job_id * 2 - (JOB_ID_SQUIRE * 2 - CHARACTER_IDENTITY_FIXED_FEMALE_SQUIRE);
        return sprite_set;
    }
    if (sprite_set == CHARACTER_IDENTITY_MONSTER) {
        *palette = unit->job_portrait_palette;
        return unit->spritesheet_id;
    }
    sprite_set = job_id + 0x28;
    return sprite_set;
}

/* Return 1 when the unit currently has any status in one row of g_status_check_sets, else 0. */
s32 status_unit_has_any_in_set(const unit_stats_t* unit, status_check_set_e status_set_index) {
    const u8* status_set = g_status_check_sets[0];
    s32 index = 0;

    status_set += status_set_index * STATUS_BYTE_COUNT;

    for (; index < STATUS_BYTE_COUNT; index++) {
        if ((unit->status_sets.current[index] & status_set[index]) != 0) {
            return 1;
        }
    }
    return 0;
}

/* Return the learned-ability bits of one job slot of career.learned_abilities
 * (0 = base job) as a 24-bit value, first byte most significant. */
u32 ability_get_learned(const unit_stats_t* unit, u32 job_index) {
    return (unit->career.learned_abilities[job_index][0] << 16) + (unit->career.learned_abilities[job_index][1] << 8)
        | unit->career.learned_abilities[job_index][2];
}

/* Copy one action command record byte by byte. An aggregate assignment would
 * replace the retail byte-copy loop. */
void action_copy_data(const action_command_t* source, action_command_t* destination) {
    const u8* from = (const u8*)source;
    u8* to = (u8*)destination;
    u32 index;

    for (index = 0; index < sizeof(*destination); index++) {
        *to++ = *from++;
    }
}

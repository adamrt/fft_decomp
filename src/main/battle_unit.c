/* Battle unit records as the resident executable builds and maintains them: a unit built for a
 * battle or Formation from a party slot or ENTD unit, an ENTD unit's generated equipment and
 * abilities, stat rebuilding and level growth, job levels and unlocks from JP, and status changes
 * and battle resets. The original compiled this as one object, with small memory, item-count,
 * random and zodiac helpers among the unit code. */

#include "fft/game/battle_unit.h"
#include "fft/battle/action.h"
#include "fft/battle/encounter.h"
#include "fft/game/ability.h"
#include "fft/game/character_identity.h"
#include "fft/game/entd.h"
#include "fft/game/item.h"
#include "fft/game/job.h"
#include "fft/game/party.h"
#include "fft/game/party_unit.h"
#include "fft/game/skillset.h"
#include "fft/game/status.h"
#include "fft/gfx/zodiac.h"
#include "fft/script/script_variable.h"
#include "fft/sys/math.h"
#include "fft/sys/memory.h"
#include "fft/sys/system.h"
#include "fft/text/text.h"
#include "psx/libc.h"
#include "psx/types.h"

extern job_data_t g_job_data[JOB_ID_COUNT];
extern u16 g_calendar_month_start_day_offsets[13];
extern const u16 g_zodiac_day_limits[ZODIAC_SIGN_ORDINARY_COUNT];

static u8 s_party_highest_level;
static u32 s_names_use_world_text = 0;

static void finish_build(battle_unit_t* unit);
static void set_ramza_identity(battle_unit_t* unit);
static s32 set_encounter_data(battle_unit_t* unit, const entd_unit_t* entd);
static s32 init_encounter_party_member(battle_unit_t* unit, const entd_unit_t* entd_unit);
static void copy_party_record(battle_unit_t* unit, const party_unit_t* party);
static void set_encounter_jobs(battle_unit_t* unit, const entd_unit_t* entd);
static s32 choose_unlocked_job(const battle_unit_t* unit);
static void copy_job_data(battle_unit_t* unit);
static void enable_rsm_flags(battle_unit_t* unit);
static void set_rsm_flag(battle_unit_t* unit, u32 ability_id);
static void calculate_base_stats(battle_unit_t* unit, s32 mode);
static void set_encounter_abilities(battle_unit_t* unit, const entd_unit_t* entd);
static void set_encounter_equipment(battle_unit_t* unit, const entd_unit_t* entd);
static void add_equippable_types(battle_unit_t* unit);
static void set_equipment_stats(battle_unit_t* unit);
static void set_equipment_attributes(battle_unit_t* unit, s32 level_up_check);
static void apply_movement_bonuses(battle_unit_t* unit, s32 keep_hp_mp);
static void store_names(battle_unit_t* unit);
static void learn_job_abilities(battle_unit_t* unit, s32 job_id, const entd_unit_t* entd);
static u16 select_rsm(const battle_unit_t* unit, u16 ability_id, s32 flags, const entd_unit_t* entd);
static void init_timers(battle_unit_t* unit);
static void reconcile_statuses(battle_unit_t* unit);
static void refresh_stats(battle_unit_t* unit, s32 skip_status_check, s32 initializing);
static void reset_battle_state(battle_unit_t* unit);
static u8 resolve_sprite_identity(const battle_unit_t* unit, u8* palette);

/* building units */

/* Rebuild one map unit from its persistent party slot.
 *
 * use_world_text selects the name-text provider used by the shared final
 * setup. Returns -1 when the requested party slot is unavailable. */
s32 battle_unit_init_from_party_slot(battle_unit_t* unit, s32 slot, s32 use_world_text) {
    party_unit_t* party_unit;

    s_names_use_world_text = use_world_text;
    party_unit = party_unit_get(slot);
    if (party_unit == 0) {
        return -1;
    }
    if (party_unit->party_id == PARTY_ID_NONE) {
        return -1;
    }
    job_calculate_levels_from_jp(&party_unit->career.job_points.total, party_unit->career.job_levels);
    job_store_unlocks(party_unit->career.unlocked_jobs,
        job_calculate_unlocks(party_unit->career.job_levels, party_unit->gender_flags));
    reset_battle_state(unit);
    unit->formation_index = slot;
    copy_party_record(unit, party_unit);
    copy_job_data(unit);
    enable_rsm_flags(unit);
    calculate_base_stats(unit, 0);
    add_equippable_types(unit);
    finish_build(unit);
    return 0;
}

/* Build one map unit from an ENTD definition.
 *
 * use_world_text selects the WORLD name provider during the shared final
 * setup. A temporary formation index enables level growth for generated units.
 * UNIT_ENTD_INIT_TRANSFORM is the transformed-monster recalculation path
 * and preserves the existing runtime/ENTD-derived state. */
s32 battle_unit_init_from_encounter(battle_unit_t* unit, const entd_unit_t* entd, u32 use_world_text, s32 kind) {
    s_names_use_world_text = use_world_text;
    if (kind != UNIT_ENTD_INIT_TRANSFORM) {
        reset_battle_state(unit);
        if (set_encounter_data(unit, entd) != 0) {
            return -1;
        }
    }
    /* For a new unit, a load-formation unit and a Ramza without a birth month came whole
     * from the roster in set_encounter_data. */
    if (kind == UNIT_ENTD_INIT_NEW) {
        if (entd->unit_flags & UNIT_FLAG_LOAD_FORMATION) {
            return 0;
        }
        if (entd->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
            if (entd->birth_month == 0) {
                return 0;
            }
        }
    }
    set_encounter_jobs(unit, entd);
    set_encounter_abilities(unit, entd);
    enable_rsm_flags(unit);
    battle_unit_generate_raw_stats(unit);
    battle_unit_copy_job_growths(unit);
    if (unit->formation_index == UNIT_FORMATION_INDEX_NONE) {
        unit->formation_index = UNIT_FORMATION_INDEX_GENERATED_UNIT;
    }
    calculate_base_stats(unit, 0);
    if (unit->formation_index == UNIT_FORMATION_INDEX_GENERATED_UNIT) {
        unit->formation_index = UNIT_FORMATION_INDEX_NONE;
    }
    add_equippable_types(unit);
    if (kind != UNIT_ENTD_INIT_TRANSFORM) {
        if (unit->formation_index >= PARTY_ROSTER_SLOT_COUNT) {
            set_encounter_equipment(unit, entd);
        }
    }
    finish_build(unit);
    set_ramza_identity(unit);
    return 0;
}

/* The shared last steps of building a unit from a party slot or from ENTD data: apply
 * its equipment stats and attributes, work out Move and Jump, and store its names. */
static void finish_build(battle_unit_t* unit) {
    set_equipment_stats(unit);
    set_equipment_attributes(unit, 1);
    apply_movement_bonuses(unit, 0);
    store_names(unit);
}

/* For a unit with one of Ramza's three identities, copy the name, birthday and
 * zodiac sign from the first roster record that has one of them. */
static void set_ramza_identity(battle_unit_t* unit) {
    s32 slot;
    party_unit_t* party;

    if (unit->character_identity > CHARACTER_IDENTITY_RAMZA_CHAPTER_4) {
        return;
    }
    for (slot = 0; slot < PARTY_ROSTER_SLOT_COUNT; slot++) {
        party = party_unit_get(slot);
        if (party->party_id == PARTY_ID_NONE) {
            continue;
        }
        if (party->sprite_set > CHARACTER_IDENTITY_RAMZA_CHAPTER_4) {
            continue;
        }
        unit->birthday.birthday = party->birthday_day + (party->zodiac << 8);
        unit->birthday.zodiac = party->zodiac >> PARTY_ZODIAC_SHIFT;
        mem_copy_byte_data_source_first(party->name, unit->name, sizeof(unit->name));
        return;
    }
}

/* Initialize the ENTD-controlled portion of a map unit.
 *
 * Formation-loading entries reuse their persistent party unit; other entries
 * derive identity, level, birthday, position, rewards, and AI fields from the
 * encounter definition. The AI-byte copy intentionally uses the global current
 * ENTD pointer, matching the target even if it differs from entd.
 *
 * Level 0 or 0xfe is random in [L - L / 8, L] for the highest party level L,
 * and 100 or more means L + (level - 100); the result is kept within 1..99. A
 * birthday whose month is outside 1..12 or day outside 1..31 becomes a random
 * day of the year. Brave and Faith above 100 are rolled in 45..74, and
 * experience of 100 or more in 0..99. */
static s32 set_encounter_data(battle_unit_t* unit, const entd_unit_t* entd) {
    const job_data_t* ramza_jobs;
    party_unit_t* party_unit;
    s32 party_slot;
    s32 party_index;
    s32 i;
    s32 pre_chapter_4_job;
    s32 level_range;
    s32 year_day;
    /* One temporary: the highest party level, then the birthday month. */
    u32 number;
    s32 level_base;
    s32 zodiac;
    u8 value;
    u8 sprite;
    u8 level;
    /* Pin required: unpinned, day is loaded into $a0 instead of $s0 (it does
     * not cross a call, and reusing `value` keeps its zero-extension). */
    register u8 day __asm__("$16");
    u8 skillset;
    u8 level_bonus;
    u16 birthday;

    unit->sprite_palette = entd->palette;
    unit->team_flags = entd->team_flags;
    unit->initial_team_flags = entd->team_flags;

    /* An ENTD Ramza without a birth month stands for the roster's Ramza: each roster Ramza
     * takes this chapter's identity, and its job too while that is his chapter 1 or 2-3
     * Squire job; a secondary skillset from one of his three Squire jobs becomes this
     * chapter job's skillset. He is then loaded from the roster like a load-formation
     * unit. */
    value = entd->unit_flags;
    if (value & UNIT_FLAG_LOAD_FORMATION) {
        party_slot = init_encounter_party_member(unit, entd);
    } else if (entd->sprite_set < CHARACTER_IDENTITY_RAMZA_END && entd->birth_month == 0) {
        for (party_index = 0, ramza_jobs = &g_job_data[1]; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
            party_unit = party_unit_get(party_index);
            if (party_unit->party_id == PARTY_ID_NONE) {
                continue;
            }
            if (party_unit->sprite_set >= CHARACTER_IDENTITY_RAMZA_END) {
                continue;
            }
            pre_chapter_4_job = party_unit->job_id < JOB_ID_SQUIRE_RAMZA_CHAPTER_4;
            party_unit->sprite_set = entd->sprite_set;
            skillset = party_unit->secondary_skillset;
            if (pre_chapter_4_job) {
                party_unit->job_id = entd->sprite_set;
            }
            if (skillset == ramza_jobs[0].skillset || skillset == ramza_jobs[1].skillset
                || skillset == ramza_jobs[2].skillset) {
                party_unit->secondary_skillset = g_job_data[entd->sprite_set].skillset;
            }
            party_slot = init_encounter_party_member(unit, entd);
            value = UNIT_FLAG_LOAD_FORMATION;
        }
    }

    for (i = 0; i < UNIT_AI_SETUP_BYTE_COUNT; i++) {
        unit->ai.bytes[i] = g_encounter_current_unit->ai.bytes[i];
    }
    if (value & UNIT_FLAG_LOAD_FORMATION) {
        return party_slot;
    }

    unit->unit_flags = value;
    sprite = entd->sprite_set;
    unit->formation_index = UNIT_FORMATION_INDEX_NONE;
    unit->character_identity = sprite;

    value = entd->level;
    if (value == 0 || value == ENTD_VALUE_RANDOM) {
        number = s_party_highest_level;
        level_range = (number >> 3) + 1;
        level_base = number - (number >> 3);
        value = level_base + (level_range * rand()) / 0x8000;
    } else if (value >= 100) {
        level_bonus = s_party_highest_level - 100;
        value += level_bonus;
    }
    level = value;
    if (level == 0) {
        level = 1;
    }
    value = level;
    if (value > UNIT_LEVEL_MAX) {
        value = UNIT_LEVEL_MAX;
    }
    unit->level = value;

    number = entd->birth_month;
    day = entd->birth_day;
    if (number == 0 || number >= 13 || day == 0 || day >= 32) {
        birthday = (365 * rand()) / 0x8000 + 1;
    } else {
        birthday = g_calendar_month_start_day_offsets[number] + day;
    }
    year_day = birthday;
    unit->birthday.birthday = year_day;
    /* Preserve the target's explicit 16-bit normalization before packing the
     * zodiac nibble into the 16-bit birthday value. */
    zodiac = zodiac_calculate_sign(year_day) & 0xFFFF;
    unit->birthday.zodiac = zodiac;
    if (entd->job_id == JOB_ID_SERPENTARIUS) {
        unit->birthday.zodiac = ZODIAC_SIGN_SERPENTARIUS;
    }

    value = entd->brave;
    if (value >= 0x65) {
        value = (30 * rand()) / 0x8000 + 45;
    }
    unit->brave = value;
    unit->original_brave = value;
    value = entd->faith;
    if (value >= 0x65) {
        value = (30 * rand()) / 0x8000 + 45;
    }
    unit->faith = value;
    unit->original_faith = value;
    value = entd->experience;
    if (value >= UNIT_LEVEL_UP_EXPERIENCE) {
        value = (UNIT_LEVEL_UP_EXPERIENCE * rand()) / 0x8000;
    }
    unit->experience = value;
    unit->quote_name_id.value = entd->name_id;
    return 0;
}

/* Load the party unit selected by an ENTD sprite set.
 *
 * Preserve the party unit's kind flags while taking the ENTD persistence
 * controls and team assignment. Returns -1 when no matching party slot exists. */
static s32 init_encounter_party_member(battle_unit_t* unit, const entd_unit_t* entd_unit) {
    s32 slot = party_find_slot_by_sprite_set(entd_unit->sprite_set);

    if (slot != -1) {
        battle_unit_init_from_party_slot(unit, slot, s_names_use_world_text);
        /* Keep the party unit's kind flags and take the two persistence
         * controls from the ENTD definition. */
        unit->unit_flags &= ~(UNIT_FLAG_JOIN_AFTER_EVENT | UNIT_FLAG_SAVE_FORMATION);
        unit->unit_flags |= entd_unit->unit_flags & (UNIT_FLAG_JOIN_AFTER_EVENT | UNIT_FLAG_SAVE_FORMATION);
        unit->team_flags = entd_unit->team_flags;
        unit->initial_team_flags = entd_unit->team_flags;
        return 0;
    }
    return -1;
}

/*
 * Fills an in-battle unit record from its persistent party record. The unit
 * is a player-controlled blue unit; Ramza's identities also get importance 3
 * (the whole UNIT_TEAM_IMPORTANCE_MASK), so losing him decides the battle. A
 * roster member (slot below 16) gets palette 0, while a guest keeps its saved palette.
 */
static void copy_party_record(battle_unit_t* unit, const party_unit_t* party) {
    s32 team = UNIT_TEAM_BLUE | UNIT_TEAM_FLAG_PLAYER_CONTROLLED;
    s32 i;
    s32 value;

    if (party->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
        team = UNIT_TEAM_BLUE | UNIT_TEAM_FLAG_PLAYER_CONTROLLED | UNIT_TEAM_IMPORTANCE_MASK;
    }
    unit->team_flags = team;
    unit->initial_team_flags = team;
    unit->character_identity = party->sprite_set;
    unit->formation_index = party->party_id;
    unit->job_id = party->job_id;
    value = party->palette;
    if (party->party_id < PARTY_MEMBER_SLOT_COUNT) {
        value = 0;
    }
    unit->sprite_palette = value;
    unit->unit_flags = party->gender_flags;
    value = party->birthday_day + (party->zodiac << 8);
    unit->birthday.birthday = value;
    unit->birthday.zodiac = party->zodiac >> PARTY_ZODIAC_SHIFT;
    unit->secondary_skillset = party->secondary_skillset;
    unit->abilities.reaction = party->reaction_ability.low + (party->reaction_ability.high << 8);
    unit->abilities.support = party->support_ability.low + (party->support_ability.high << 8);
    unit->abilities.movement = party->movement_ability.low + (party->movement_ability.high << 8);
    mem_copy_byte_data_source_first(party->equipment, unit->equipment, UNIT_EQUIPMENT_SLOT_COUNT);
    unit->experience = party->experience;
    unit->level = party->level;
    unit->original_brave = unit->brave = party->brave;
    unit->original_faith = unit->faith = party->faith;
    mem_copy_byte_data_source_first(party->raw_stats, unit->raw_stats, sizeof(unit->raw_stats));
    for (i = 0; i < UNIT_UNLOCKED_JOB_BYTE_COUNT; i++) {
        unit->career.unlocked_jobs[i] = party->career.unlocked_jobs[i];
    }
    mem_copy_byte_data_source_first(
        party->career.learned_abilities, unit->career.learned_abilities, UNIT_LEARNED_ABILITY_BYTE_COUNT);
    mem_copy_byte_data_source_first(party->career.job_levels, unit->career.job_levels, UNIT_JOB_LEVEL_BYTE_COUNT);
    /* Copy the adjacent current/total JP tables as their 80 serialized bytes. */
    mem_copy_byte_data_source_first(
        &party->career.job_points, &unit->career.job_points, sizeof(unit->career.job_points));
    mem_copy_byte_data_source_first(party->name, unit->name, sizeof(unit->name));
    unit->quote_name_id.value = party->name_id.index + (party->name_id.name_class << 8);
    mem_zero(&unit->ai, sizeof(unit->ai));
}

/* Set an ENTD unit's job levels, unlocked jobs, job record and skillsets.
 *
 * A human gets job levels that meet the unlock requirements of its job_unlock
 * career job, that job at job_level, and at least level 1 in every job but the
 * Bard or Dancer its sex cannot take; its unlocked jobs follow from those
 * levels. A monster unlocks nothing. The ENTD primary skillset, unless 0 or
 * SKILLSET_ID_NONE, replaces the job's. A random secondary skillset
 * (ENTD_VALUE_RANDOM) is that of a random unlocked job, or none for a monster
 * or when it matches the primary. With a null entd the unit keeps its job
 * record, loses its secondary skillset and unlocked jobs, and becomes a
 * monster. */
static void set_encounter_jobs(battle_unit_t* unit, const entd_unit_t* entd) {
    s32 i;
    u32 unlocked;
    u32 flags;
    u8 selected;
    u16 skillset;
    u8 levels;

    mem_zero(unit->career.job_levels, UNIT_JOB_LEVEL_BYTE_COUNT);
    unlocked = 0;
    if (!entd) {
        copy_job_data(unit);
        unit->secondary_skillset = 0;
        unit->unit_flags = UNIT_FLAG_MONSTER;
        job_store_unlocks(unit->career.unlocked_jobs, 0);
    } else {
        flags = entd->unit_flags;
        if (!(flags & UNIT_FLAG_MONSTER)) {
            selected = entd->job_unlock;
            if (selected < UNIT_CAREER_JOB_COUNT) {
                levels = entd->job_level & UNIT_JOB_LEVEL_LOW_NIBBLE_MASK;
                if (!(selected & 1))
                    levels <<= 4;
                unit->career.job_levels[selected / UNIT_JOBS_PER_LEVEL_BYTE] = levels;
                for (i = 0; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
                    if (selected)
                        levels = g_job_unlock_requirements[selected - 1][i];
                    else {
                        levels = UNIT_JOB_LEVEL_ONE_LOW_NIBBLE;
                        if (i != 0)
                            levels = UNIT_JOB_LEVEL_ONE_BOTH_NIBBLES;
                    }
                    levels |= unit->career.job_levels[i];
                    if (!(levels & UNIT_JOB_LEVEL_LOW_NIBBLE_MASK))
                        levels |= UNIT_JOB_LEVEL_ONE_LOW_NIBBLE;
                    if (!(levels & UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK))
                        levels |= UNIT_JOB_LEVEL_ONE_HIGH_NIBBLE;
                    unit->career.job_levels[i] = levels;
                }
                if (flags & UNIT_FLAG_FEMALE)
                    unit->career.job_levels[UNIT_JOB_LEVEL_BYTE_INDEX_CALCULATOR_BARD]
                        &= UNIT_JOB_LEVEL_HIGH_NIBBLE_MASK;
                if (flags & UNIT_FLAG_MALE)
                    unit->career.job_levels[UNIT_JOB_LEVEL_BYTE_INDEX_DANCER_MIME] &= UNIT_JOB_LEVEL_LOW_NIBBLE_MASK;
                unlocked = job_calculate_unlocks(unit->career.job_levels, flags);
            }
        }
        job_store_unlocks(unit->career.unlocked_jobs, unlocked);
        unit->job_id = entd->job_id;
        copy_job_data(unit);
        selected = entd->primary_skillset;
        if (selected != SKILLSET_ID_NONE && selected != 0) {
            unit->primary_skillset = selected;
            unit->ability_outcome = 0xff;
        }
        selected = entd->secondary_skillset;
        if (selected == ENTD_VALUE_RANDOM) {
            if (unit->unit_flags & UNIT_FLAG_MONSTER)
                unit->secondary_skillset = 0;
            else {
                /* The job-level temporary holds the random job id here. */
                levels = choose_unlocked_job(unit);
                if (levels)
                    skillset = g_job_data_pointer[levels].skillset;
                else
                    skillset = levels;
                if (skillset == unit->primary_skillset)
                    skillset = 0;
                unit->secondary_skillset = skillset;
            }
        } else
            unit->secondary_skillset = selected;
    }
}

enum { RANDOM_JOB_COUNT = JOB_ID_MIME - JOB_ID_GENERIC_FIRST };

/* Return a random unlocked generic job ID below Mime, or 0 when none are unlocked.
 *
 * The ENTD caller explicitly normalizes the zero-extended job byte. */
static s32 choose_unlocked_job(const battle_unit_t* unit) {
    u8 candidates[RANDOM_JOB_COUNT];
    s32 job_mask = UNIT_UNLOCKED_JOB_SQUIRE;
    s32 candidate_count = 0;
    s32 job_index;
    s32 unlocked_jobs
        = (unit->career.unlocked_jobs[0] << 16) + (unit->career.unlocked_jobs[1] << 8) + unit->career.unlocked_jobs[2];

    for (job_index = 0; job_index < RANDOM_JOB_COUNT; job_index++, job_mask /= 2) {
        if (unlocked_jobs & job_mask) {
            u8 job_id = job_index + JOB_ID_SQUIRE;

            if (job_index == 0) {
                job_id = unit->character_identity;
                if (job_id >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
                    job_id = JOB_ID_SQUIRE;
                }
            }
            candidates[candidate_count] = job_id;
            candidate_count++;
        }
    }

    if (candidate_count != 0) {
        s32 scaled_random = rand() * candidate_count;
        u8 candidate_index = (u32)scaled_random >> 15;

        if (scaled_random < 0) {
            candidate_index = (u32)(scaled_random + 0x7fff) >> 15;
        }
        return candidates[candidate_index];
    }
    return 0;
}

/* Load a battle unit's job-derived fields from its job record: skillsets, innate
 * abilities, equippable types, growths and multipliers, Move, Jump, the innate,
 * immunity and starting status sets, elemental affinities and graphics ids.
 *
 * base_job_skillset is the skillset of the job whose id equals a special
 * character's identity, and 0 for a generic unit. The job's stepping-stone bit
 * moves from the Jump byte to the unit's position byte. An immortal unit that
 * also has a save- or load-formation flag gains the immunity check sets 5 and 6;
 * the tests inside that loop repeat the outer condition. The strengthened
 * elements are cleared. */
static void copy_job_data(battle_unit_t* unit) {
    job_data_t* job;
    s32 i;
    s32 immortal;
    s32 formation_flags;

    if (unit->character_identity < CHARACTER_IDENTITY_GENERIC_FIRST) {
        unit->base_job_skillset = g_job_data_pointer[unit->character_identity].skillset;
    } else {
        unit->base_job_skillset = 0;
    }

    job = g_job_data_pointer + unit->job_id;
    unit->primary_skillset = job->skillset;
    mem_copy_byte_data_source_first(job->innate_abilities, unit->innate_abilities, sizeof(unit->innate_abilities));
    mem_copy_byte_data_source_first(
        job->equipment_categories, unit->equipment_categories, sizeof(unit->equipment_categories));
    mem_copy_byte_data_source_first(job->stat_growths, unit->stat_growths, sizeof(unit->stat_growths));
    unit->move = job->move;
    unit->jump = job->jump & JOB_JUMP_VALUE_MASK;
    if (job->jump & JOB_JUMP_STEPPING_STONE) {
        unit->position.stepping_stone = 1;
    } else {
        unit->position.stepping_stone = 0;
    }
    mem_copy_byte_data_source_first(&job->status_sets, &unit->status_sets, sizeof(job->status_sets));

    immortal = unit->team_flags & UNIT_TEAM_FLAG_IMMORTAL;
    formation_flags = unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION);
    if (immortal != 0 && formation_flags != 0) {
        u8* statuses = unit->status_sets.immunity;
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if (immortal != 0) {
                statuses[i] |= g_status_check_sets[STATUS_CHECK_SET_IMMORTAL_IMMUNITY][i];
            }
            if (formation_flags != 0) {
                statuses[i] |= g_status_check_sets[STATUS_CHECK_SET_FORMATION_IMMUNITY][i];
            }
        }
    }

    mem_copy_byte_data_source_first(job->elemental_affinity, unit->elemental_affinity, sizeof(job->elemental_affinity));
    unit->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN] = 0;
    unit->spritesheet_id = job->spritesheet_id;
    unit->job_portrait_palette = job->job_portrait_palette;
    unit->monster_graphic = job->monster_graphic;
}

/* Rebuilds the unit's active reaction/support/movement bitfields
 * (0x8b..0x95) from its four innate abilities and equipped R/S/M
 * abilities. */
static void enable_rsm_flags(battle_unit_t* unit) {
    s32 i;

    mem_zero(&unit->ability_flags, sizeof(unit->ability_flags));
    for (i = 0; i < 4; i++) {
        set_rsm_flag(unit, unit->innate_abilities[i]);
    }
    set_rsm_flag(unit, unit->abilities.reaction);
    set_rsm_flag(unit, unit->abilities.support);
    set_rsm_flag(unit, unit->abilities.movement);
}

/* Mark an R/S/M ability active and select reaction abilities.
 *
 * IDs 0x1a6..0x1fd map contiguously into the unit's MSB-first active flags.
 * A reaction ID also becomes the unit's currently equipped reaction ability. */
static void set_rsm_flag(battle_unit_t* unit, u32 ability_id) {
    u16 index = ability_id - ABILITY_ID_REACTION_FIRST;
    u16 original_ability_id = ability_id;
    u8* rsm_flags;

    ability_id = index;
    if (index < ABILITY_ID_RANDOM_FIRST - ABILITY_ID_REACTION_FIRST) {
        if (original_ability_id < ABILITY_ID_SUPPORT_FIRST) {
            unit->abilities.reaction = original_ability_id;
        }
        rsm_flags = (u8*)&unit->ability_flags;
        rsm_flags += ability_id >> 3;
        *rsm_flags |= 0x80 >> (ability_id & 7);
    }
}

/* Derive max HP, max MP, Speed, PA and MA from the 24-bit raw stats: raw * job
 * multiplier / 1638400, at least 1; HP and MP at most 999, Speed at most 50,
 * PA and MA at most 99. An egg's HP and MP are ten times that, capped at
 * 0xffff.
 *
 * Mode 0 also fills HP and MP and, for a generated unit, first replays the
 * growth of levels 2 to its level (raw += raw / (growth + level - 1) for
 * each). A mode with bit 0 set leaves Speed, PA and MA as they are. */
static void calculate_base_stats(battle_unit_t* unit, s32 mode) {
    u32 value;
    u32 multiplier;
    s32 hp_mp_divisor = 100;
    s32 hp_mp_max = UNIT_HP_MP_MAX;
    s32 i;
    s32 j;
    u8 level;
    s32 keep_base_attributes = mode & 1;
    s32 divisor;
    s32 is_low;
    job_stat_growth_t* mult;
    unit_raw_stat_t* raw;

    if (unit->unit_flags & UNIT_FLAG_EGG) {
        hp_mp_divisor = 10;
        hp_mp_max = 0xffff;
    }
    mult = unit->stat_growths;
    level = unit->level;
    raw = unit->raw_stats;

    for (i = 0; i < UNIT_RAW_STAT_COUNT; i++) {
        value = raw[i].low + (raw[i].middle << 8) + (raw[i].high << 16);
        if (mode == 0 && unit->formation_index == UNIT_FORMATION_INDEX_GENERATED_UNIT) {
            divisor = mult[i].growth;
            divisor = divisor ? divisor : 1;
            for (j = 2; j <= level; j++) {
                value += value / (divisor + j - 1);
            }
        }
        divisor = 100;
        if (value > 0xffffff) {
            value = 0xffffff;
        }
        raw[i].middle = value >> 8;
        is_low = i < UNIT_RAW_STAT_SPEED;
        raw[i].low = value;
        raw[i].high = value >> 16;
        multiplier = mult[i].multiplier;
        if (is_low) {
            divisor = hp_mp_divisor;
        }
        value = (value * multiplier) / divisor >> 14;
        if (value == 0) {
            value = 1;
        }
        if (is_low) {
            if (value > hp_mp_max) {
                value = hp_mp_max;
            }
        }
        if (i == UNIT_RAW_STAT_SPEED && value > UNIT_SPEED_MAX) {
            value = UNIT_SPEED_MAX;
        }
        if (i >= UNIT_RAW_STAT_PHYSICAL_ATTACK && value > UNIT_ATTACK_STAT_MAX) {
            value = UNIT_ATTACK_STAT_MAX;
        }
        switch (i) {
        case UNIT_RAW_STAT_HP:
            unit->max_hp = value;
            if (mode == 0) {
                unit->hp = value;
            }
            break;
        case UNIT_RAW_STAT_MP:
            unit->max_mp = value;
            if (mode == 0) {
                unit->mp = value;
            }
            break;
        case UNIT_RAW_STAT_SPEED:
            if (keep_base_attributes == 0) {
                unit->base_attributes[UNIT_ATTRIBUTE_SPEED] = value;
            }
            break;
        case UNIT_RAW_STAT_PHYSICAL_ATTACK:
            if (keep_base_attributes == 0) {
                unit->base_attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK] = value;
            }
            break;
        case UNIT_RAW_STAT_MAGIC_ATTACK:
            if (keep_base_attributes == 0) {
                unit->base_attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK] = value;
            }
            break;
        }
    }
}

/* Fills a unit's ability data from its ENTD entry: primary skillset, random
 * JP and learned abilities for each generic job (humans only), the job's
 * innate abilities and equipment categories, and the R/S/M abilities.
 *
 * Random JP for a job level: g_job_jp_requirements_by_level[level] (the
 * JP needed to reach it; 0x80066182, entry 0 is 0) plus rand(0..99), or
 * 100 + rand(0..99) for level 0. */
static void set_encounter_abilities(battle_unit_t* unit, const entd_unit_t* entd) {
    u8 job_id = unit->job_id;
    u8 gender = unit->unit_flags;
    u8 byte;
    s32 i;
    s32 level;
    u16 value;

    if (entd == 0) {
        mem_zero(&unit->abilities, sizeof(unit->abilities));
        return;
    }

    byte = entd->primary_skillset;
    if (byte != 0xff && byte != 0) {
        unit->primary_skillset = byte;
    } else {
        unit->primary_skillset = g_job_data_pointer[job_id].skillset;
    }

    if (gender & (UNIT_FLAG_MALE | UNIT_FLAG_FEMALE)) {
        mem_zero(unit->career.learned_abilities, UNIT_LEARNED_ABILITY_BYTE_COUNT);
        for (i = 0; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
            byte = unit->career.job_levels[i];
            level = byte >> 4;
            if (i == UNIT_JOB_LEVEL_BYTE_COUNT - 1 && (gender & UNIT_FLAG_MALE)) {
                value = 0; /* Dancer is female only */
            } else if (level != 0) {
                value = g_job_jp_requirements_by_level[level] + rand() * 100 / 0x8000;
            } else {
                value = rand() * 100 / 0x8000 + 100;
            }
            unit->career.job_points.total.values[i * 2] = value;
            unit->career.job_points.current.values[i * 2] = value;
            learn_job_abilities(unit, i * 2 + JOB_ID_SQUIRE, entd);

            level = byte & 0xf;
            if (i == UNIT_JOB_LEVEL_BYTE_COUNT - 2 && (gender & UNIT_FLAG_FEMALE)) {
                value = 0; /* Bard is male only */
            } else if (level != 0) {
                value = g_job_jp_requirements_by_level[level] + rand() * 100 / 0x8000;
            } else {
                value = rand() * 100 / 0x8000 + 100;
            }
            unit->career.job_points.total.values[i * 2 + 1] = value;
            unit->career.job_points.current.values[i * 2 + 1] = value;
            if (i == UNIT_JOB_LEVEL_BYTE_COUNT - 1) {
                break; /* Mime's JP is initialized, but its learning call is skipped. */
            }
            learn_job_abilities(unit, i * 2 + JOB_ID_SQUIRE + 1, entd);
        }

        if (entd->primary_skillset != 0 && entd->primary_skillset != SKILLSET_ID_NONE) {
            unit->career.learned_abilities[0][0] = 0xff;
            unit->career.learned_abilities[0][1] = 0xff;
            unit->career.learned_abilities[0][2] = 0xff;
        }
    }

    for (i = 0; i < 4; i++) {
        s32 k = i * 2;

        unit->innate_abilities[i] = g_job_data_pointer[job_id].innate_abilities[k]
            + (g_job_data_pointer[job_id].innate_abilities[k + 1] << 8);
        unit->equipment_categories[i] = g_job_data_pointer[job_id].equipment_categories[i];
    }

    unit->abilities.reaction = select_rsm(unit, entd->abilities.reaction, RSM_ABILITY_FILTER_REACTION, entd);
    unit->abilities.support = select_rsm(unit, entd->abilities.support, RSM_ABILITY_FILTER_SUPPORT, entd);
    unit->abilities.movement = select_rsm(unit, entd->abilities.movement, RSM_ABILITY_FILTER_MOVEMENT, entd);
}

/* Map ENTD's five equipment bytes into the seven simulation slots. */
static void set_monster_equipment(battle_unit_t* unit, const entd_unit_t* entd) {
    unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = entd->equipment[ENTD_EQUIPMENT_SLOT_RIGHT_HAND];
    unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = ITEM_ID_NONE;
    unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = entd->equipment[ENTD_EQUIPMENT_SLOT_LEFT_HAND];
    unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = ITEM_ID_NONE;
    unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD] = entd->equipment[ENTD_EQUIPMENT_SLOT_HEAD];
    unit->equipment[UNIT_EQUIPMENT_SLOT_BODY] = entd->equipment[ENTD_EQUIPMENT_SLOT_BODY];
    unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] = entd->equipment[ENTD_EQUIPMENT_SLOT_ACCESSORY];
}

/* Fill a battle unit's seven equipment slots from its ENTD record, rolling a
 * random item for each slot the record marks ENTD_VALUE_RANDOM.
 *
 * A monster takes set_monster_equipment instead. The random right
 * hand tries, in order, a Two Hands weapon, a Two Swords weapon, then the type of
 * each Equip weapon support the unit has (sword, katana, crossbow, spear, axe,
 * gun), then any weapon. The left hand is emptied when the right-hand weapon
 * needs both hands or the unit has Two Hands; otherwise it rolls a Two Swords
 * weapon, then a shield if the unit can equip one. Either hand's item goes to
 * that hand's weapon or shield slot by its kind. Head and body prefer a helmet or
 * armor when the support test passes, then any headgear or body armor. A roll
 * that finds nothing leaves the slot empty, and item 0 is stored as ITEM_ID_NONE.
 *
 * The body-armor preference tests Equip Shield, not Equip Armor. */
static void set_encounter_equipment(battle_unit_t* unit, const entd_unit_t* entd) {
    u8 item;
    u8 flags;
    s32 i;
    u8 right_weapon;

    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        set_monster_equipment(unit, entd);
        return;
    }

    /* Right hand */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_RIGHT_HAND];
    flags = unit->ability_flags.support[2];
    if (item == ENTD_VALUE_RANDOM) {
        if (flags & UNIT_SUPPORT_SET_3_TWO_HANDS) {
            item = battle_unit_generate_random_equipment(
                unit, ITEM_TYPE_FLAG_WEAPON, WEAPON_FLAG_TWO_HANDS_COMPATIBLE, ITEM_TYPE_FILTER_ANY);
        }
        if (item == ENTD_VALUE_RANDOM && (flags & UNIT_SUPPORT_SET_3_TWO_SWORDS)) {
            item = battle_unit_generate_random_equipment(
                unit, ITEM_TYPE_FLAG_WEAPON, WEAPON_FLAG_TWO_SWORDS_COMPATIBLE, ITEM_TYPE_FILTER_ANY);
        }
    }

    /* Then each Equip weapon support in turn, then any weapon. */
    flags = unit->ability_flags.support[0];
    if (item == ENTD_VALUE_RANDOM) {
        if (flags & UNIT_SUPPORT_SET_1_EQUIP_SWORD) {
            item = battle_unit_generate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_SWORD);
        }
        if (item == ENTD_VALUE_RANDOM) {
            if (flags & UNIT_SUPPORT_SET_1_EQUIP_KATANA) {
                item = battle_unit_generate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_KATANA);
            }
            if (item == ENTD_VALUE_RANDOM) {
                if (flags & UNIT_SUPPORT_SET_1_EQUIP_CROSSBOW) {
                    item = battle_unit_generate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_CROSSBOW);
                }
                if (item == ENTD_VALUE_RANDOM) {
                    if (flags & UNIT_SUPPORT_SET_1_EQUIP_SPEAR) {
                        item = battle_unit_generate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_SPEAR);
                    }
                    if (item == ENTD_VALUE_RANDOM) {
                        if (flags & UNIT_SUPPORT_SET_1_EQUIP_AXE) {
                            item = battle_unit_generate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_AXE);
                        }
                        if (item == ENTD_VALUE_RANDOM) {
                            if (flags & UNIT_SUPPORT_SET_1_EQUIP_GUN) {
                                item = battle_unit_generate_random_equipment(
                                    unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_GUN);
                            }
                            if (item == ENTD_VALUE_RANDOM) {
                                item = battle_unit_generate_random_equipment(
                                    unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_FILTER_ANY);
                                if (item == ENTD_VALUE_RANDOM) {
                                    item = ITEM_ID_NONE;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (g_item_primary_data[item].type_flags & ITEM_TYPE_FLAG_SHIELD) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item;
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
    } else {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item;
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = ITEM_ID_NONE;
    }

    /* Left hand: empty beside a weapon that needs both hands, or with Two Hands. */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_LEFT_HAND];
    right_weapon = unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON];
    if (right_weapon != ITEM_ID_NONE
        && ((g_item_weapon_data[g_item_primary_data[right_weapon].secondary_data_id].flags
                & WEAPON_FLAG_FORCED_TWO_HANDS)
            || (unit->ability_flags.support[2] & UNIT_SUPPORT_SET_3_TWO_HANDS))) {
        item = ITEM_ID_NONE;
    }

    flags = unit->ability_flags.support[2];
    if (item == ENTD_VALUE_RANDOM && (flags & UNIT_SUPPORT_SET_3_TWO_SWORDS)) {
        item = battle_unit_generate_random_equipment(
            unit, ITEM_TYPE_FLAG_WEAPON, WEAPON_FLAG_TWO_SWORDS_COMPATIBLE, ITEM_TYPE_FILTER_ANY);
    }
    flags = unit->equipment_categories[EQUIPPABLE_ITEM_TYPE_BYTE_INDEX(ITEM_TYPE_SHIELD)];
    if (item == ENTD_VALUE_RANDOM) {
        if (flags & EQUIPPABLE_ITEM_TYPE_BYTE_MASK(ITEM_TYPE_SHIELD)) {
            item = battle_unit_generate_random_equipment(unit, ITEM_TYPE_FLAG_SHIELD, 0, ITEM_TYPE_FILTER_ANY);
        } else {
            item = ITEM_ID_NONE;
        }
    }

    if (g_item_primary_data[item].type_flags & ITEM_TYPE_FLAG_WEAPON) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item;
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = ITEM_ID_NONE;
    } else {
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item;
        unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = ITEM_ID_NONE;
    }

    /* Head */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_HEAD];
    flags = unit->ability_flags.support[0];
    if (item == ENTD_VALUE_RANDOM) {
        if (flags & UNIT_SUPPORT_SET_1_EQUIP_ARMOR) {
            item = battle_unit_generate_random_equipment(unit, ITEM_TYPE_FLAG_HEADGEAR, 0, ITEM_TYPE_HELMET);
        }
        if (item == ENTD_VALUE_RANDOM) {
            item = battle_unit_generate_random_equipment(unit, ITEM_TYPE_FLAG_HEADGEAR, 0, ITEM_TYPE_FILTER_ANY);
            if (item == ENTD_VALUE_RANDOM) {
                item = ITEM_ID_NONE;
            }
        }
    }
    unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD] = item;

    /* Body: flags still holds support set 1 from the head. */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_BODY];
    if (item == ENTD_VALUE_RANDOM) {
        /* The target reuses Equip Shield here while requesting random Armor;
         * preserve this native equipment-generation quirk. */
        if (flags & UNIT_SUPPORT_SET_1_EQUIP_SHIELD) {
            item = battle_unit_generate_random_equipment(unit, ITEM_TYPE_FLAG_BODY_ARMOR, 0, ITEM_TYPE_ARMOR);
        }
        if (item == ENTD_VALUE_RANDOM) {
            item = battle_unit_generate_random_equipment(unit, ITEM_TYPE_FLAG_BODY_ARMOR, 0, ITEM_TYPE_FILTER_ANY);
            if (item == ENTD_VALUE_RANDOM) {
                item = ITEM_ID_NONE;
            }
        }
    }
    unit->equipment[UNIT_EQUIPMENT_SLOT_BODY] = item;

    /* Accessory */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_ACCESSORY];
    if (item == ENTD_VALUE_RANDOM) {
        item = battle_unit_generate_random_equipment(unit, ITEM_TYPE_FLAG_ACCESSORY, 0, ITEM_TYPE_FILTER_ANY);
        if (item == ENTD_VALUE_RANDOM) {
            item = ITEM_ID_NONE;
        }
    }
    unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] = item;

    /* An empty slot is ITEM_ID_NONE (255) in a battle unit, never item 0. */
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        if (unit->equipment[i] == ITEM_ID_NOTHING) {
            unit->equipment[i] = ITEM_ID_NONE;
        }
    }
}

/* Add to the unit's equippable item types those its Equip support abilities grant
 * (Equip Armor covers helmets and armor), and the ribbon, bag and perfume types
 * for a female unit. */
static void add_equippable_types(battle_unit_t* unit) {
    s32 support = unit->ability_flags.support[0];

    if (support & UNIT_SUPPORT_SET_1_EQUIP_ARMOR) {
        unit->equipment_categories[2] |= UNIT_EQUIPMENT_SET_3_HELM | UNIT_EQUIPMENT_SET_3_ARMOR;
    }
    if (support & UNIT_SUPPORT_SET_1_EQUIP_SHIELD) {
        unit->equipment_categories[2] |= UNIT_EQUIPMENT_SET_3_SHIELD;
    }
    if (support & UNIT_SUPPORT_SET_1_EQUIP_SWORD) {
        unit->equipment_categories[0] |= UNIT_EQUIPMENT_SET_1_SWORD;
    }
    if (support & UNIT_SUPPORT_SET_1_EQUIP_KATANA) {
        unit->equipment_categories[0] |= UNIT_EQUIPMENT_SET_1_KATANA;
    }
    if (support & UNIT_SUPPORT_SET_1_EQUIP_CROSSBOW) {
        unit->equipment_categories[1] |= UNIT_EQUIPMENT_SET_2_CROSSBOW;
    }
    if (support & UNIT_SUPPORT_SET_1_EQUIP_SPEAR) {
        unit->equipment_categories[1] |= UNIT_EQUIPMENT_SET_2_SPEAR;
    }
    if (support & UNIT_SUPPORT_SET_1_EQUIP_AXE) {
        unit->equipment_categories[0] |= UNIT_EQUIPMENT_SET_1_AXE;
    }
    if (support & UNIT_SUPPORT_SET_1_EQUIP_GUN) {
        unit->equipment_categories[1] |= UNIT_EQUIPMENT_SET_2_GUN;
    }
    support = unit->unit_flags;
    if (support & UNIT_FLAG_FEMALE) {
        unit->equipment_categories[2] |= UNIT_EQUIPMENT_SET_3_RIBBON | UNIT_EQUIPMENT_SET_3_BAG;
        unit->equipment_categories[3] |= UNIT_EQUIPMENT_SET_4_PERFUME;
    }
}

/* Fill unit->equipment_stats from the equipped items: the job's class evade, the
 * accessory's and each shield's physical and magic evade, and each weapon's power
 * and evade. A slot whose item is not of the expected kind leaves zeros, and a
 * monster gets only the class evade. */
static void set_equipment_stats(battle_unit_t* unit) {
    item_data_t* item;
    item_evade_t* shield;
    weapon_data_t* weapon;

    mem_zero(unit->equipment_stats, sizeof(unit->equipment_stats));
    unit->equipment_stats[UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE] = g_job_data_pointer[unit->job_id].class_evade;
    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        return;
    }

    /* Each secondary id is read by indexing g_item_primary_data again: reading it through `item`
     * folds both loads onto one base register. */
    item = &g_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]];
    if (item->type_flags & ITEM_TYPE_FLAG_ACCESSORY) {
        item_evade_t* accessory
            = &g_item_accessory_data[g_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]]
                    .secondary_data_id];
        unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE] = accessory->physical_evade;
        unit->equipment_stats[UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE] = accessory->magic_evade;
    }
    item = &g_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]];
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        shield = &g_item_shield_data[g_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]]
                .secondary_data_id];
        unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE] = shield->physical_evade;
        unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE] = shield->magic_evade;
    }
    item = &g_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]];
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        shield = &g_item_shield_data[g_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]]
                .secondary_data_id];
        unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE] = shield->physical_evade;
        unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE] = shield->magic_evade;
    }
    item = &g_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]];
    if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
        weapon = &g_item_weapon_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]];
        unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER] = weapon->power;
        unit->equipment_stats[UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE] = weapon->evade;
    }
    item = &g_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]];
    if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
        weapon = &g_item_weapon_data[unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]];
        unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER] = weapon->power;
        unit->equipment_stats[UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE] = weapon->evade;
    }
}

/* Recompute a unit's stats with its equipment. Max HP and MP come from the raw
 * stats again (with level_up_check set, the base Speed, PA and MA too). Each
 * equipped item then adds its attribute record: PA, MA and Speed bonuses (each
 * total capped at 255), Move (capped at 0xfd) and Jump (at 7), its innate,
 * immunity and starting statuses to the unit's innate, immunity and current
 * sets, and its elemental affinities; headgear and body armor add their HP and
 * MP bonuses (max 999). A monster takes nothing from equipment. HP and MP are
 * clamped to the new maxima, and the actual PA and MA are base + bonus capped
 * at 99, Speed at 50. Also sets the Materia Blade and sword equipped flags. */
static void set_equipment_attributes(battle_unit_t* unit, s32 level_up_check) {
    s32 i;
    s32 j;
    u32 value;
    u32 cap;
    u8 item_id;
    u32 second_id;
    item_data_t* item;
    item_attribute_t* attribute;
    u8* bonus; /* equipment bonuses, later reused to walk the base/actual stats */
    u8* source;
    u8* target;

    mem_zero(unit->equipment_attributes, sizeof(unit->equipment_attributes));
    cap = UNIT_HP_MP_MAX;
    calculate_base_stats(unit, level_up_check + 1);
    unit->equipped_flags = 0;
    if (!(unit->unit_flags & UNIT_FLAG_MONSTER)) {
        bonus = unit->equipment_attributes;
        for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
            item_id = unit->equipment[i];
            if (item_id == ITEM_ID_MATERIA_BLADE) {
                unit->equipped_flags |= UNIT_EQUIPPED_FLAG_MATERIA_BLADE;
            }
            if (item_id < ITEM_ID_SHIELD_FIRST) {
                if (g_item_primary_data[item_id].type == ITEM_TYPE_SWORD
                    || g_item_primary_data[item_id].type == ITEM_TYPE_KNIGHT_SWORD) {
                    unit->equipped_flags |= UNIT_EQUIPPED_FLAG_SWORD;
                }
            }
            if (item_id == ITEM_ID_NONE) {
                continue;
            }
            item = &g_item_primary_data[item_id];
            attribute = &g_item_attributes[item->attributes];
            source = attribute->attributes;
            for (j = 0; j < 3; j++) {
                value = source[j] + bonus[j];
                if (value >= 0x100) {
                    value = 0xff;
                }
                bonus[j] = value;
            }
            value = attribute->move + unit->move;
            if (value >= 0xfe) {
                value = 0xfd;
            }
            unit->move = value;
            value = attribute->jump + unit->jump;
            if (value >= 8) {
                value = 7;
            }
            unit->jump = value;
            /* One 15-byte pass covers all three status sets. */
            j = 0;
            target = (u8*)&unit->status_sets;
            for (; j < 15; j++) {
                *target = attribute->innate[j] | *target;
                target++;
            }
            j = 0;
            target = unit->elemental_affinity;
            for (; j < ELEMENTAL_AFFINITY_COUNT; j++) {
                *target = attribute->elemental_affinity[j] | *target;
                target++;
            }
            if (item->type_flags & (ITEM_TYPE_FLAG_HEADGEAR | ITEM_TYPE_FLAG_BODY_ARMOR)) {
                second_id = item->secondary_data_id;
                value = unit->max_hp + g_item_helm_armor_secondary_data[second_id].hp_bonus;
                if (value > cap) {
                    value = cap;
                }
                unit->max_hp = value;
                value = unit->max_mp + g_item_helm_armor_secondary_data[second_id].mp_bonus;
                if (value > cap) {
                    /* The original routes the cap through the loop scratch; a plain
                     * `value = cap` changes the register allocation. */
                    value = j = cap;
                }
                unit->max_mp = value;
            }
        }
    }
    if (unit->hp > unit->max_hp) {
        unit->hp = unit->max_hp;
    }
    if (unit->mp > unit->max_mp) {
        unit->mp = unit->max_mp;
    }
    /* base_attributes, equipment_attributes and attributes lie 3 bytes apart. */
    bonus = unit->base_attributes;
    for (j = 0; j < 3; j++) {
        value = bonus[j + 3] + bonus[j];
        if (j < 2) {
            if (value > UNIT_ATTACK_STAT_MAX) {
                value = UNIT_ATTACK_STAT_MAX;
            }
        } else {
            if ((u16)value > UNIT_SPEED_MAX) {
                value = UNIT_SPEED_MAX;
            }
        }
        bonus[j + 6] = value;
    }
}

/* Add the Move +1/+2/+3 and Jump +1/+2/+3 abilities to Move and Jump; Move is
 * capped at 0xFC and Jump at 7. With keep_hp_mp 0, HP and MP are refilled
 * first. */
static void apply_movement_bonuses(battle_unit_t* unit, s32 keep_hp_mp) {
    u16 move;
    u16 jump;
    u8 flags;

    if (keep_hp_mp == 0) {
        unit->hp = unit->max_hp;
        unit->mp = unit->max_mp;
    }
    move = unit->move;
    flags = unit->ability_flags.movement[0];
    jump = unit->jump;
    if (flags & UNIT_MOVEMENT_SET_1_MOVE_PLUS_1) {
        move += 1;
    }
    if (flags & UNIT_MOVEMENT_SET_1_MOVE_PLUS_2) {
        move += 2;
    }
    if (flags & UNIT_MOVEMENT_SET_1_MOVE_PLUS_3) {
        move += 3;
    }
    if (flags & UNIT_MOVEMENT_SET_1_JUMP_PLUS_1) {
        jump += 1;
    }
    if (flags & UNIT_MOVEMENT_SET_1_JUMP_PLUS_2) {
        jump += 2;
    }
    if (flags & UNIT_MOVEMENT_SET_1_JUMP_PLUS_3) {
        jump += 3;
    }
    if (move >= 0xFD) {
        move = 0xFC;
    }
    if (jump >= 8) {
        jump = 7;
    }
    unit->move = move;
    unit->jump = jump;
}

enum { NAME_GENDER_MASK = UNIT_FLAG_MALE | UNIT_FLAG_FEMALE | UNIT_FLAG_MONSTER };

/* Copy the unit's name, job name and skillset names into its battle record,
 * from BATTLE's or WORLD's text as g_names_use_world_text says. A
 * roster unit keeps its saved name. Any other unit takes the special-character
 * name quote_name_id selects, or, when that is 0xff, a random generic name of
 * its kind that no roster member or battle unit of the same kind has, drawn as
 * in generate_record (index 255 is never drawn). The action's
 * status changes are cleared too. */
static void store_names(battle_unit_t* unit) {
    /* Removing this unused array reduces the native stack frame by 0x100. */
    u8 unused[0x100];
    u8* (*prepare_text)(u32 text_id);
    u32 text_id;
    unit_name_id_t name_base;
    u8 gender;
    unit_name_id_t name_id;
    unit_name_id_t existing_name;
    u8 name_index;
    s32 name_available;
    s32 i;
    u8* text;
    party_unit_t* party;
    battle_unit_t* other;
    const unit_name_id_fields_t* name_id_record;

    prepare_text = text_find_battle_entry;
    if (s_names_use_world_text != 0) {
        prepare_text = text_find_world_entry;
    }
    if (unit->formation_index == PARTY_ID_NONE) {
        text_id = TEXT_ID_UNIT_NAME_SPECIAL_BASE;
        name_index = unit->quote_name_id.value;
        name_id.value = name_index;
        if (name_id.value == 0xff) {
            gender = unit->unit_flags & NAME_GENDER_MASK;
            if (unit->unit_flags & UNIT_FLAG_MALE) {
                text_id = TEXT_ID_UNIT_NAME_GENERIC_MALE_BASE;
                name_base.value = 0;
                name_base.fields.name_class = PARTY_NAME_CLASS_GENERIC_MALE;
            } else if (unit->unit_flags & UNIT_FLAG_FEMALE) {
                text_id = TEXT_ID_UNIT_NAME_GENERIC_FEMALE_BASE;
                name_base.value = 0;
                name_base.fields.name_class = PARTY_NAME_CLASS_GENERIC_FEMALE;
            } else {
                text_id = TEXT_ID_UNIT_NAME_GENERIC_MONSTER_BASE;
                name_base.value = 0;
                name_base.fields.name_class = PARTY_NAME_CLASS_GENERIC_MONSTER;
            }
            do {
                name_available = 1;
                name_id.value = name_base.value + (rand() * 255) / 0x8000;
                for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
                    party = &g_party_units[i];
                    if (party->party_id != PARTY_ID_NONE && gender == (party->gender_flags & NAME_GENDER_MASK)) {
                        name_id_record = &party->name_id;
                        existing_name.value = 0;
                        existing_name.fields.name_class = name_id_record->name_class;
                        existing_name.fields.index = name_id_record->index;
                        if (existing_name.value == name_id.value) {
                            name_available = 0;
                            break;
                        }
                    }
                }
                for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                    other = &g_battle_units[i];
                    if (gender == (other->unit_flags & NAME_GENDER_MASK)
                        && other->quote_name_id.value == name_id.value) {
                        name_available = 0;
                        break;
                    }
                }
            } while (name_available == 0);
            unit->quote_name_id.value = name_id.value;
        }
        name_index = name_id.fields.index;
        text_id += name_index;
        text = prepare_text(text_id);
    } else {
        text = party_unit_get(unit->formation_index)->name;
    }
    mem_copy_bytes_source_first(text, unit->name, sizeof(unit->name));
    text_id = unit->job_id | TEXT_ID_JOB_NAME_BASE;
    mem_copy_bytes_source_first(prepare_text(text_id), unit->job_name, sizeof(unit->job_name));
    text_id = unit->primary_skillset | TEXT_ID_SKILLSET_NAME_BASE;
    mem_copy_bytes_source_first(
        prepare_text(text_id), unit->primary_skillset_name, sizeof(unit->primary_skillset_name));
    text_id = unit->secondary_skillset | TEXT_ID_SKILLSET_NAME_BASE;
    mem_copy_bytes_source_first(
        prepare_text(text_id), unit->secondary_skillset_name, sizeof(unit->secondary_skillset_name));
    mem_zero(&unit->action.status_changes, sizeof(unit->action.status_changes));
}

/* Return the highest level in the roster, capped at 99, and store it in
 * g_party_highest_level. */
u8 party_calculate_highest_level(void) {
    u8 highest = 0;
    s32 i;
    party_unit_t* unit;

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = party_unit_get(i);
        if (unit->party_id != PARTY_ID_NONE && highest < unit->level) {
            highest = unit->level;
        }
    }
    if (highest > UNIT_LEVEL_MAX) {
        highest = UNIT_LEVEL_MAX;
    }
    s_party_highest_level = highest;
    return highest;
}

/* equipment and ability generation */

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
s32 battle_unit_generate_random_equipment(
    const battle_unit_t* unit, u8 item_type_flag, u8 weapon_flag_mask, u8 required_item_type) {
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
        if (!(unit->equipment_categories[EQUIPPABLE_ITEM_TYPE_BYTE_INDEX(type)]
                & EQUIPPABLE_ITEM_TYPE_BYTE_MASK(type))) {
            continue;
        }
        if (weapon_flag_mask != 0) {
            if (!(weapon_flag_mask & g_item_weapon_data[item->secondary_data_id].flags)) {
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
static void learn_job_abilities(battle_unit_t* unit, s32 job_id, const entd_unit_t* entd) {
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
        if (!(scratch & i)) {
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
        ability_id = skillset_get_ability_id(skillset, i);
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
static u16 select_rsm(const battle_unit_t* unit, u16 ability_id, s32 flags, const entd_unit_t* entd) {
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
            id = skillset_get_ability_id(skillset, j);
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

/* stats and level growth */

/* Seed inflicted_status with the current statuses the unit does not have
 * innately, then set up the status CTs (init_timers) and reconcile the
 * status flags. */
static void init_statuses(battle_unit_t* unit) {
    s32 status_set;

    for (status_set = 0; status_set < STATUS_BYTE_COUNT; status_set++) {
        unit->inflicted_status[status_set]
            = unit->status_sets.current[status_set] & ~unit->status_sets.innate[status_set];
    }
    init_timers(unit);
    reconcile_statuses(unit);
}

/* Clear the timed-status CTs and start the death counter at 3, or at 0xff for
 * an immortal unit and for one with a save- or load-formation flag. */
static void init_timers(battle_unit_t* unit) {
    mem_zero(unit->status_ct, sizeof(unit->status_ct));
    if ((unit->team_flags & UNIT_TEAM_FLAG_IMMORTAL)
        || (unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION))) {
        unit->death_counter = 0xff;
    } else {
        unit->death_counter = 3;
    }
}

/* Apply Float and reconcile the unit's status flags and status CT. */
static void reconcile_statuses(battle_unit_t* unit) {
    s32 status;
    u8 immunity;

    if (unit->ability_flags.movement[2] & UNIT_MOVEMENT_SET_3_FLOAT) {
        unit->status_sets.innate[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] |= STATUS_BYTE_MASK(STATUS_ID_FLOAT);
    }

    for (status = 0; status < STATUS_BYTE_COUNT; status++) {
        /* The bytes need the immunity byte staged through a local; `&=` schedules differently. */
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

/* refresh_stats with skip_status_check set: stats
 * are re-derived without the critical and status notifications. */
void battle_unit_refresh_formation_stats(battle_unit_t* unit) {
    refresh_stats(unit, 1, 0);
}

/* refresh_stats with both flags clear: stats are
 * re-derived and the status notifications run. */
void battle_unit_refresh_stats(battle_unit_t* unit) {
    refresh_stats(unit, 0, 0);
}

/* refresh_stats in its initializing form: the
 * inflicted statuses are seeded from the current ones and their CTs cleared.
 * Nothing in the tree calls it. */
void battle_unit_init(battle_unit_t* unit) {
    refresh_stats(unit, 0, 1);
}

/* Re-derives the job-dependent stats (move/jump, equippable categories,
 * innate/immune/starting statuses, elemental affinity, R/S/M flags and
 * equipment bonuses), then reconciles the status bits.  `skip_status_check`
 * leaves the critical/status notifications alone; `initializing` seeds the
 * inflicted statuses from the current ones and clears their CTs. */
static void refresh_stats(battle_unit_t* unit, s32 skip_status_check, s32 initializing) {
    u8 saved_inflicted[STATUS_BYTE_COUNT];
    u8 saved_current[STATUS_BYTE_COUNT];
    s32 i;
    s32 status_id;
    s32 mask;
    s32 byte_index;
    s32 current_bit;
    s32 battle_unit_id = unit->battle_unit_id;
    s32 saved_bit;
    u8 formation_index;
    job_data_t* job;

    status_refresh_current(unit);
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
    enable_rsm_flags(unit);
    apply_movement_bonuses(unit, 1);
    set_equipment_stats(unit);
    set_equipment_attributes(unit, 0);
    add_equippable_types(unit);

    for (i = 0; i < STATUS_BYTE_COUNT; i++) {
        if (initializing) {
            unit->inflicted_status[i] = unit->status_sets.current[i];
        } else {
            unit->inflicted_status[i] = saved_inflicted[i];
        }
    }

    formation_index = unit->formation_index;
    unit->formation_index = UNIT_FORMATION_INDEX_NONE;
    reconcile_statuses(unit);
    unit->formation_index = formation_index;
    status_refresh_current(unit);

    if (skip_status_check != 0) {
        return;
    }
    if (unit->max_hp / 5 >= unit->hp) {
        unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)] |= STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
    } else {
        unit->inflicted_status[STATUS_BYTE_INDEX(STATUS_ID_CRITICAL)] &= ~STATUS_BYTE_MASK(STATUS_ID_CRITICAL);
    }
    status_refresh_current(unit);

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
                status_queue_display_flag_change_if_executing(status_id, STATUS_DISPLAY_CHANGE_ADD, battle_unit_id);
            } else {
                status_queue_display_flag_change_if_executing(status_id, STATUS_DISPLAY_CHANGE_REMOVE, battle_unit_id);
            }
        }
    }
}

/* Level up at 100 EXP: one level of growth, and EXP goes back to 0 (the excess
 * is lost). At level 99 EXP is held at 99 instead. Returns 1 when the level
 * rose. */
s32 battle_unit_try_level_up(battle_unit_t* unit) {
    u8 level;

    if (unit->experience < UNIT_LEVEL_UP_EXPERIENCE) {
        return 0;
    }

    level = unit->level;
    if (level >= UNIT_LEVEL_MAX) {
        unit->experience = UNIT_LEVEL_EXPERIENCE_MAX;
        return 0;
    }

    battle_unit_apply_level_growth(unit, 0);
    unit->experience = 0;
    unit->level = level + 1;
    return 1;
}

/* Raises a roster unit by `level_delta` levels (capped at 99), growing each
 * 24-bit raw stat by raw / (level + growth) per level, and returns the new
 * level.  The growth table is the job's stat_growths pairs. */
u32 party_unit_gain_levels(party_unit_t* party_unit, s32 level_delta) {
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
void battle_unit_apply_level_growth(battle_unit_t* unit, s32 level_down) {
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

    battle_unit_refresh_stats(unit);
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
s32 status_set_ct(battle_unit_t* unit, s32 status_id, s32 removing) {
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
    /* The bytes need this signed copy: indexing with the unsigned test value is 4 bytes shorter. */
    stored_index = ct_index;
    if (removing) {
        unit->status_ct[stored_index] = 0;
        return 0;
    }
    if (stored_index == STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)
        && unit->status_ct[STATUS_CT_INDEX(STATUS_ID_DEATH_SENTENCE)] != 0) {
        return -1;
    }
    unit->status_ct[stored_index] = g_status_effect_data[status_id].ct;
    return 0;
}

/* job progress */

/* Compute a unit's unlocked-job bitset (Squire in bit 23, always set) from its packed job levels:
 * a job unlocks when every level nibble meets its g_job_unlock_requirements row. The other
 * sex's Bard or Dancer level counts as maxed for the check, then that job is masked out; a monster
 * unlocks nothing. */
u32 job_calculate_unlocks(const u8* packed_job_levels, u32 gender_flags) {
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
    party = party_unit_get(party_index);
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
    if (slot & 1) {
        levels = level + (levels & 0xF0);
    } else {
        levels = (levels & 0xF) | (level_byte << 4);
    }
    party->career.job_levels[slot / 2] = levels;

    job_store_unlocks(
        party->career.unlocked_jobs, job_calculate_unlocks(party->career.job_levels, party->gender_flags));
    return 0;
}

/* Store the low 24 bits of unlocked_jobs as three bytes, most significant first. */
void job_store_unlocks(u8 destination[UNIT_UNLOCKED_JOB_BYTE_COUNT], u32 unlocked_jobs) {
    *destination++ = unlocked_jobs >> 16;
    *destination++ = unlocked_jobs >> 8;
    *destination = unlocked_jobs;
}

/* Job level: how many of the eight thresholds in
 * g_job_jp_requirements_by_level[1..8] the given JP has reached (0..8). */
u32 job_calculate_level(u16 job_jp) {
    s32 job_level = 0;
    s32 level;

    for (level = 0; level < UNIT_JOB_LEVEL_MAX; level++) {
        if (job_jp >= g_job_jp_requirements_by_level[level + 1]) {
            job_level++;
        }
    }
    return job_level;
}

/* Pack each pair of job levels from the jobs' total JP: the first job of a pair in the high
 * nibble, the second in the low. */
void job_calculate_levels_from_jp(
    const unit_job_point_table_t* unit_job_jp, u8 unit_job_levels[UNIT_JOB_LEVEL_BYTE_COUNT]) {
    s32 i;
    s32 level;

    for (i = 0; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
        level = job_calculate_level(unit_job_jp->values[i * UNIT_JOBS_PER_LEVEL_BYTE]) << 4;
        unit_job_levels[i] = level | job_calculate_level(unit_job_jp->values[i * UNIT_JOBS_PER_LEVEL_BYTE + 1]);
    }
}

/* unit state and helpers */

/* Clear the per-battle unit state: manual auto-battle with no target, CT 0, no active turn or
 * ability outcome, no charging ability (0xff), not mounted and no KOs. */
static void reset_battle_state(battle_unit_t* unit) {
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
 * initialize the unit's status data (init_statuses). */
void battle_unit_init_status_and_rewards(battle_unit_t* unit, s32 clear_rewards) {
    if (unit->attributes[UNIT_ATTRIBUTE_SPEED] == 0) {
        unit->attributes[UNIT_ATTRIBUTE_SPEED] = 1;
    }
    if (clear_rewards != 0) {
        unit->war_trophy = ITEM_ID_NOTHING;
        unit->bonus_money_modifier = 0;
    }
    init_statuses(unit);
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

/* Resolve the unit's spritesheet and palette (resolve_sprite_identity), store both in
 * the unit and return the spritesheet id. */
u8 battle_unit_set_sprite_identity(battle_unit_t* unit, u8* palette) {
    u8 spritesheet = resolve_sprite_identity(unit, palette);

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
static u8 resolve_sprite_identity(const battle_unit_t* unit, u8* palette) {
    u8 job_id;
    u8 sprite_set;

    *palette = 0;
    job_id = unit->job_id;
    sprite_set = unit->character_identity;
    if (job_id == JOB_ID_DANCER || job_id == JOB_ID_MIME) {
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
s32 status_has_any_in_check_set(const battle_unit_t* unit, status_check_set_e status_set_index) {
    const u8* status_set = g_status_check_sets[0];
    s32 index = 0;

    status_set += status_set_index * STATUS_BYTE_COUNT;

    for (; index < STATUS_BYTE_COUNT; index++) {
        if (unit->status_sets.current[index] & status_set[index]) {
            return 1;
        }
    }
    return 0;
}

/* Return the learned-ability bits of one job slot of career.learned_abilities
 * (0 = base job) as a 24-bit value, first byte most significant. */
u32 ability_get_learned(const battle_unit_t* unit, u32 job_index) {
    return (unit->career.learned_abilities[job_index][0] << 16) + (unit->career.learned_abilities[job_index][1] << 8)
        | unit->career.learned_abilities[job_index][2];
}

/* Copy one action command record byte by byte. An aggregate assignment would
 * replace the retail byte-copy loop. */
void action_command_copy(const action_command_t* source, action_command_t* destination) {
    const u8* from = (const u8*)source;
    u8* to = (u8*)destination;
    u32 index;

    for (index = 0; index < sizeof(*destination); index++) {
        *to++ = *from++;
    }
}

/* A second native entry with the same copy loop as mem_copy_bytes_source_first. */
void mem_copy_byte_data_source_first(const void* source, void* destination, int count) {
    const u8* src = source;
    u8* dst = destination;
    int i;

    for (i = 0; i < count; i++) {
        *dst++ = *src++;
    }
}

/* Count the copies of an item the player owns: the inventory stock plus every roster human's
 * equipped copies. With include_battle set, roster humans deployed in the battle are counted from
 * their battle records instead, along with every other non-Mime human that started on the blue
 * team. */
s32 item_get_total_quantity(u8 item_id, s32 include_battle) {
    s32 total = g_item_quantities[item_id];
    s32 i;
    s32 j;
    party_unit_t* party;
    battle_unit_t* unit;
    s32 deployed;
    s32 party_id;

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        party = party_unit_get(i);
        party_id = party->party_id;
        if (party_id == PARTY_ID_NONE) {
            continue;
        }
        if (party->gender_flags & UNIT_FLAG_MONSTER) {
            continue;
        }
        if (include_battle != 0) {
            deployed = 0;
            for (j = 0; j < BATTLE_UNIT_SLOT_COUNT; j++) {
                unit = &g_battle_units[j];
                if (unit->existence != UNIT_EXISTENCE_NONE && !(unit->unit_flags & UNIT_FLAG_MONSTER)
                    && unit->formation_index == party_id) {
                    deployed = 1;
                    break;
                }
            }
            if (deployed != 0) {
                continue;
            }
        }
        for (j = 0; j < UNIT_EQUIPMENT_SLOT_COUNT; j++) {
            if (party->equipment[j] == item_id) {
                total++;
            }
        }
    }

    if (include_battle == 0) {
        return total;
    }

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_units[i];
        if (unit->existence != UNIT_EXISTENCE_NONE && !(unit->unit_flags & UNIT_FLAG_MONSTER)
            && !(unit->initial_team_flags & UNIT_TEAM_MASK) && unit->job_id != JOB_ID_MIME) {
            for (j = 0; j < UNIT_EQUIPMENT_SLOT_COUNT; j++) {
                if (unit->equipment[j] == item_id) {
                    total++;
                }
            }
        }
    }
    return total;
}

/* Copy the ENTD unit's map X and Y, elevation, facing and spell-quote bits, war trophy and bonus
 * money into the battle unit. */
void battle_unit_init_position_and_rewards(battle_unit_t* unit, const entd_unit_t* entd) {
    unit->x = entd->x;
    unit->position.y = entd->position.y;
    unit->position.higher_elevation = entd->position.higher_elevation;
    unit->position.facing = entd->position.facing;
    unit->position.spell_quote_skillsets = entd->position.spell_quote_skillsets;

    unit->war_trophy = entd->war_trophy;
    unit->bonus_money_modifier = entd->bonus_money_modifier;
}

/*
 * Return the highest-order status changed by an action. Bits 0-6 encode the
 * one-based status index, 0x80 marks removal, and 0x100 reflects flags_2 bit 3.
 */
s32 status_find_action_priority(const action_data_t* action) {
    s32 highest_order = -1;
    s32 result = 0;
    s32 status_index;
    u8 mask;
    s32 byte_index;
    u8 current_order;

    for (status_index = 0; status_index < STATUS_COUNT; status_index++) {
        byte_index = status_index / 8;
        mask = 0x80 >> (status_index & 7);
        if (action->status_changes.removal[byte_index] & mask) {
            current_order = g_status_effect_data[status_index].order;
            if (current_order > highest_order) {
                highest_order = current_order;
                result = status_index + 1 + STATUS_ACTION_RESULT_REMOVAL;
                if (g_status_effect_data[status_index].flags_2 & STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED) {
                    result
                        = status_index + 1 + STATUS_ACTION_RESULT_REMOVAL + STATUS_ACTION_RESULT_SCRIPT_FILTER_EXCLUDED;
                }
            }
        }
        if (action->status_changes.infliction[byte_index] & mask) {
            current_order = g_status_effect_data[status_index].order;
            if (current_order > highest_order) {
                highest_order = current_order;
                result = status_index + 1;
                if (g_status_effect_data[status_index].flags_2 & STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED) {
                    result = status_index + 1 + STATUS_ACTION_RESULT_SCRIPT_FILTER_EXCLUDED;
                }
            }
        }
    }
    return result;
}

/* Return the zodiac sign for a day-of-year birthday.
 *
 * Counts the sign boundaries at or before the day, then rotates the count
 * into zodiac order with (count + 9) % 12. */
s32 zodiac_calculate_sign(u16 birthday) {
    s32 zodiac = 0;
    s32 i;

    for (i = 0; i < ZODIAC_SIGN_ORDINARY_COUNT; i++) {
        zodiac += birthday >= g_zodiac_day_limits[i];
    }
    zodiac = (zodiac + 9) % ZODIAC_SIGN_ORDINARY_COUNT;
    return (u16)zodiac;
}

/* Clear count bytes one at a time; a count of 0 or less clears nothing. */
void mem_zero(void* destination, s32 count) {
    u8* dst = destination;
    s32 index = 0;

    while (index < count) {
        *dst++ = 0;
        index++;
    }
}

/* Count a KO in the script variables: a unit that started on the blue team adds to
 * SCRIPT_VAR_CASUALTIES, any other unit to SCRIPT_VAR_INJURED; both stop at 9999. */
void battle_unit_record_casualty(const battle_unit_t* unit) {
    s32 id;
    s32 count;

    id = (unit->initial_team_flags & UNIT_TEAM_MASK) != UNIT_TEAM_BLUE ? SCRIPT_VAR_INJURED : SCRIPT_VAR_CASUALTIES;
    count = script_variable_get(id);
    if (count < EVENT_CASUALTY_COUNT_MAX) {
        count++;
    }
    script_variable_set(id, count);
}

/* Add, remove, or replace a status-set flag and refresh the unit's current status. */
void status_change_unit_flag(battle_unit_t* unit, s32 status_byte_index, u8 status_flag, s32 change_type) {
    u8 updated;

    /* An enum selector removes the native signed range test. */
    switch (change_type) {
    case STATUS_ADD:
        updated = unit->inflicted_status[status_byte_index] | status_flag;
        break;
    case STATUS_REMOVE:
        updated = unit->inflicted_status[status_byte_index] & ~status_flag;
        break;
    case STATUS_REPLACE:
        updated = status_flag;
        break;
    }

    unit->inflicted_status[status_byte_index] = updated;
    unit->status_sets.current[status_byte_index] = updated | unit->status_sets.innate[status_byte_index];
}

/* Recompute the current status set as innate | inflicted. */
void status_refresh_current(battle_unit_t* unit) {
    s32 status_set;

    for (status_set = 0; status_set < STATUS_BYTE_COUNT; status_set++) {
        unit->status_sets.current[status_set]
            = unit->status_sets.innate[status_set] | unit->inflicted_status[status_set];
    }
}

/* Charge time of the unit's last ability: the low seven bits of the ability
 * record's CT byte. */
void battle_unit_set_ability_ct(battle_unit_t* unit) {
    unit->ability_ct = g_ability_range_data[unit->action_command.ability_id].ct & 0x7f;
}

/* Make Charging, Jump, Defending or Performing the unit's only action status, or clear all four
 * with STATUS_ACTION_STATE_NONE, and enable or disable the special flags of each status that
 * changed. NONE and DEFENDING also clear the charging ability CT; PERFORMING sets it from the last
 * ability's CT. KEEP_PERFORMING keeps Performing if the unit has it and otherwise acts as NONE. */
void status_set_action_state(battle_unit_t* unit, u8 action_state) {
    u8 old_status = unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)];
    s32 battle_unit_id = unit->battle_unit_id;
    u8 status;
    u8 cleared;
    s32 status_id;
    u8 mask;
    u8 changed;

    if (action_state == STATUS_ACTION_STATE_KEEP_PERFORMING) {
        /* The Performing bit (0x01) shifted left 3 is PERFORMING (8); without it, NONE (0). */
        action_state = (old_status & STATUS_BYTE_MASK(STATUS_ID_PERFORMING)) << 3;
    }
    switch (action_state) {
    case STATUS_ACTION_STATE_NONE:
        status = 0;
        unit->charged_ability_ct = 0xff;
        break;
    case STATUS_ACTION_STATE_CHARGING:
        status = STATUS_BYTE_MASK(STATUS_ID_CHARGING);
        break;
    case STATUS_ACTION_STATE_JUMPING:
        status = STATUS_BYTE_MASK(STATUS_ID_JUMP);
        break;
    case STATUS_ACTION_STATE_DEFENDING:
        status = STATUS_BYTE_MASK(STATUS_ID_DEFENDING);
        unit->charged_ability_ct = 0xff;
        break;
    case STATUS_ACTION_STATE_PERFORMING:
        status = STATUS_BYTE_MASK(STATUS_ID_PERFORMING);
        battle_unit_set_ability_ct(unit);
        unit->charged_ability_ct = unit->ability_ct;
        break;
    }
    cleared = status
        ^ (STATUS_BYTE_MASK(STATUS_ID_CHARGING) | STATUS_BYTE_MASK(STATUS_ID_JUMP)
            | STATUS_BYTE_MASK(STATUS_ID_DEFENDING) | STATUS_BYTE_MASK(STATUS_ID_PERFORMING));
    status_change_unit_flag(unit, STATUS_BYTE_INDEX(STATUS_ID_CHARGING), status, STATUS_ADD);
    status_change_unit_flag(unit, STATUS_BYTE_INDEX(STATUS_ID_CHARGING), cleared, STATUS_REMOVE);

    changed = old_status ^ unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)];
    mask = STATUS_BYTE_MASK(STATUS_ID_CHARGING);
    for (status_id = STATUS_HANDLER_INDEX(STATUS_ID_CHARGING); status_id <= STATUS_HANDLER_INDEX(STATUS_ID_PERFORMING);
        status_id++) {
        if (changed & mask) {
            if (old_status & mask) {
                status_queue_display_flag_change_if_executing(status_id, STATUS_DISPLAY_CHANGE_REMOVE, battle_unit_id);
            } else {
                status_queue_display_flag_change_if_executing(status_id, STATUS_DISPLAY_CHANGE_ADD, battle_unit_id);
            }
        }
        mask >>= 1;
    }
}

/* Pass a BATTLE.BIN unit-setup error (system_battle_load_exception_e) to the empty
 * system_report_error hook as source 3. */
void system_handle_battle_load_exception(s32 value) {
    system_report_error(3, value);
}

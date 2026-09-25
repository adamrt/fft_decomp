#include "fft/battle.h"
#include "fft/main.h"
#include "psx/types.h"

/* Initialize the ENTD-controlled portion of a runtime unit.
 *
 * Formation-loading entries reuse their persistent party unit; other entries
 * derive identity, level, birthday, position, rewards, and AI fields from the
 * encounter definition. The AI-byte copy intentionally uses the global current
 * ENTD pointer, matching the target even if it differs from entd. */
s32 main_unit_calculate_entd_data(battle_stats_t* unit, entd_unit_t* entd) {
    const job_data_t* ramza_jobs;
    party_data_t* party_unit;
    s32 party_slot;
    s32 party_index;
    s32 i;
    s32 pre_chapter_4_job;
    s32 level_range;
    /* Pin required: every unpinned spelling of the maximum leaves it out of $v1. */
    register u32 highest __asm__("$3");
    s32 year_day;
    u32 month;
    s32 level_base;
    s32 zodiac;
    u8 value;
    u8 sprite;
    u8 level;
    /* Pin required: unpinned, day is loaded into $a0 instead of $s0. */
    register u8 day __asm__("$16");
    u8 skillset;
    u8 level_bonus;
    u16 birthday;

    unit->sprite_palette = entd->palette;
    unit->team_flags = entd->battle_flags;
    unit->initial_team_flags = entd->battle_flags;

    value = entd->unit_flags;
    if (value & UNIT_FLAG_LOAD_FORMATION) {
        party_slot = main_unit_init_job_data_from_entd(unit, entd);
    } else if (entd->sprite_set < CHARACTER_IDENTITY_RAMZA_END && entd->birthday[0] == 0) {
        for (party_index = 0, ramza_jobs = &g_job_data[1]; party_index < 20; party_index++) {
            party_unit = main_party_get_data_pointer(party_index);
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
            party_slot = main_unit_init_job_data_from_entd(unit, entd);
            value = UNIT_FLAG_LOAD_FORMATION;
        }
    }

    /* Copies entd 0x21..0x27 into unit 0x165..0x16b (ai_target_x through
     * entd_unknown_right).  The byte-shifted struct view reproduces the
     * target's `addu unit,i` operand order; (&unit->ai_target_x)[i] swaps it. */
    for (i = 0; i < 7; i++) {
        ((battle_stats_t*)((u8*)unit + i))->ai_target_x = g_current_entd_unit->ai_target_xy[i];
    }
    if (value & UNIT_FLAG_LOAD_FORMATION) {
        return party_slot;
    }

    unit->unit_flags = value;
    sprite = entd->sprite_set;
    unit->formation_index = BATTLE_FORMATION_INDEX_NONE;
    unit->character_identity = sprite;

    value = entd->level;
    if (value == 0 || value == 0xfe) {
        highest = g_highest_party_level;
        level_range = (highest >> 3) + 1;
        level_base = highest - (highest >> 3);
        value = level_base + (level_range * rand()) / 0x8000;
    } else if (value >= 100) {
        level_bonus = g_highest_party_level - 100;
        value += level_bonus;
    }
    level = value;
    if (level == 0) {
        level = 1;
    }
    value = level;
    if (value >= 100) {
        value = 99;
    }
    unit->level = value;

    month = entd->birthday[0];
    day = entd->birthday[1];
    if (month == 0 || month >= 13 || day == 0 || day >= 32) {
        birthday = (365 * rand()) / 0x8000 + 1;
    } else {
        birthday = g_main_month_start_day_offsets[month] + day;
    }
    year_day = birthday;
    unit->birthday.value = (unit->birthday.value & 0xfe00) | (year_day & 0x1ff);
    /* Preserve the target's explicit 16-bit normalization before packing the
     * zodiac nibble into the birthday word. */
    zodiac = main_unit_calculate_zodiac_symbol(year_day) & 0xFFFF;
    unit->birthday.value = (unit->birthday.value & 0x0fff) | (zodiac << 12);
    if (entd->job_id == JOB_ID_SERPENTARIUS) {
        unit->birthday.value = (unit->birthday.value & 0x0fff) | (ZODIAC_SIGN_SERPENTARIUS << 12);
    }

    value = entd->bravery;
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
    if (value >= 100) {
        value = (100 * rand()) / 0x8000;
    }
    unit->experience = value;
    unit->quote_name_id = entd->name_id;
    return 0;
}

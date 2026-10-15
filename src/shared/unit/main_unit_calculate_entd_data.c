#include "fft/main.h"
#include "psx/types.h"

/* Initialize the ENTD-controlled portion of a runtime unit.
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
s32 main_unit_calculate_entd_data(unit_stats_t* unit, entd_unit_t* entd) {
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

    value = entd->unit_flags;
    if (value & UNIT_FLAG_LOAD_FORMATION) {
        party_slot = main_unit_init_job_data_from_entd(unit, entd);
    } else if (entd->sprite_set < CHARACTER_IDENTITY_RAMZA_END && entd->birth_month == 0) {
        for (party_index = 0, ramza_jobs = &g_main_job_data[1]; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
            party_unit = main_party_get_unit(party_index);
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
                party_unit->secondary_skillset = g_main_job_data[entd->sprite_set].skillset;
            }
            party_slot = main_unit_init_job_data_from_entd(unit, entd);
            value = UNIT_FLAG_LOAD_FORMATION;
        }
    }

    /* Copies entd 0x21..0x27 into unit 0x165..0x16b (ai_target_x through
     * ai_initial_enemy_flag).  The byte-shifted struct view reproduces the
     * target's `addu unit,i` operand order; (&unit->ai_target_x)[i] swaps it. */
    for (i = 0; i < 7; i++) {
        ((unit_stats_t*)((u8*)unit + i))->ai_target_x = g_main_entd_current_unit->ai_target_xy[i];
    }
    if (value & UNIT_FLAG_LOAD_FORMATION) {
        return party_slot;
    }

    unit->unit_flags = value;
    sprite = entd->sprite_set;
    unit->formation_index = BATTLE_FORMATION_INDEX_NONE;
    unit->character_identity = sprite;

    value = entd->level;
    if (value == 0 || value == ENTD_VALUE_RANDOM) {
        number = g_main_party_highest_level;
        level_range = (number >> 3) + 1;
        level_base = number - (number >> 3);
        value = level_base + (level_range * rand()) / 0x8000;
    } else if (value >= 100) {
        level_bonus = g_main_party_highest_level - 100;
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
        birthday = g_main_month_start_day_offsets[number] + day;
    }
    year_day = birthday;
    unit->birthday.birthday = year_day;
    /* Preserve the target's explicit 16-bit normalization before packing the
     * zodiac nibble into the 16-bit birthday value. */
    zodiac = main_unit_calculate_zodiac_symbol(year_day) & 0xFFFF;
    unit->birthday.zodiac = zodiac;
    if (entd->job_id == JOB_ID_SERPENTARIUS) {
        unit->birthday.zodiac = ZODIAC_SIGN_SERPENTARIUS;
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
    if (value >= UNIT_LEVEL_UP_EXPERIENCE) {
        value = (UNIT_LEVEL_UP_EXPERIENCE * rand()) / 0x8000;
    }
    unit->experience = value;
    unit->quote_name_id.value = entd->name_id;
    return 0;
}

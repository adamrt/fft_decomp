/* Calendar advances hatch eggs and roll monster offspring. Newborn monsters receive a birthday and zodiac sign before
 * their roster record is generated. */

#include "fft/calendar.h"
#include "fft/formation.h"
#include "fft/job.h"
#include "fft/party.h"
#include "fft/save.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/libc.h"
#include "psx/types.h"

/* One inclusive date limit and its zodiac ID. The 13-entry WORLD table has
 * Capricorn at both calendar ends and does not contain Serpentarius. */
typedef struct zodiac_limit {
    calendar_date_t date;
    u8 zodiac;
} zodiac_limit_t;
typedef char zodiac_limit_size_must_be_3[(sizeof(zodiac_limit_t) == 3) ? 1 : -1];

enum { ZODIAC_DATE_LIMIT_COUNT = 13 };

typedef enum rarity {
    BREEDING_RARITY_NONE = 0,
    BREEDING_RARITY_COMMON = 1,
    BREEDING_RARITY_UNCOMMON = 2,
    BREEDING_RARITY_RARE = 3
} rarity_e;

enum {
    BREEDING_RARITY_COUNT = 3,
    BREEDING_MONTH_COUNT = 13,
    BREEDING_ENTRY_COUNT = JOB_ID_TIAMAT - JOB_ID_CHOCOBO + 1
};

/* Breeding data for one monster job: its hatch-day range and offspring jobs
 * for the common, uncommon and rare rolls. */
typedef struct offspring_rule {
    u8 hatch_range;                                    /* 0x00: rand() % hatch_range + 1 (at least 2) egg days */
    u8 offspring_job_by_rarity[BREEDING_RARITY_COUNT]; /* 0x01; common through rare */
} offspring_rule_t;
typedef char offspring_rule_size_must_be_4[(sizeof(offspring_rule_t) == 4) ? 1 : -1];

static const zodiac_limit_t s_zodiac_date_limits[ZODIAC_DATE_LIMIT_COUNT] = {
    { { 1, 19 }, ZODIAC_SIGN_CAPRICORN },
    { { 2, 18 }, ZODIAC_SIGN_AQUARIUS },
    { { 3, 20 }, ZODIAC_SIGN_PISCES },
    { { 4, 19 }, ZODIAC_SIGN_ARIES },
    { { 5, 20 }, ZODIAC_SIGN_TAURUS },
    { { 6, 21 }, ZODIAC_SIGN_GEMINI },
    { { 7, 22 }, ZODIAC_SIGN_CANCER },
    { { 8, 22 }, ZODIAC_SIGN_LEO },
    { { 9, 22 }, ZODIAC_SIGN_VIRGO },
    { { 10, 23 }, ZODIAC_SIGN_LIBRA },
    { { 11, 22 }, ZODIAC_SIGN_SCORPIO },
    { { 12, 22 }, ZODIAC_SIGN_SAGITTARIUS },
    { { 12, 31 }, ZODIAC_SIGN_CAPRICORN },
};

/* Days per month, indexed from 1; the leading zero is outside the sum. */
static const u8 s_breeding_month_lengths[BREEDING_MONTH_COUNT] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/* Each three-job monster family shares common/uncommon offspring. The first
 * job's rare result stays at that job; the other two use the third job. */
#define BREEDING_FAMILY(job, first_range, second_range, third_range)                                                   \
    { first_range, { job, job + 1, job } }, { second_range, { job, job + 1, job + 2 } }, {                             \
        third_range, { job, job + 1, job + 2 }                                                                         \
    }
static const offspring_rule_t s_breeding_table[BREEDING_ENTRY_COUNT] = {
    BREEDING_FAMILY(JOB_ID_CHOCOBO, 3, 4, 5),
    BREEDING_FAMILY(JOB_ID_GOBLIN, 3, 4, 5),
    BREEDING_FAMILY(JOB_ID_BOMB, 3, 4, 5),
    BREEDING_FAMILY(JOB_ID_RED_PANTHER, 3, 4, 5),
    BREEDING_FAMILY(JOB_ID_PISCO_DEMON, 3, 4, 6),
    BREEDING_FAMILY(JOB_ID_SKELETON, 3, 4, 5),
    BREEDING_FAMILY(JOB_ID_GHOUL, 3, 4, 5),
    BREEDING_FAMILY(JOB_ID_FLOTIBALL, 3, 5, 6),
    BREEDING_FAMILY(JOB_ID_JURAVIS, 3, 4, 5),
    BREEDING_FAMILY(JOB_ID_URIBO, 3, 3, 3),
    BREEDING_FAMILY(JOB_ID_WOODMAN, 3, 4, 5),
    BREEDING_FAMILY(JOB_ID_BULL_DEMON, 4, 5, 6),
    BREEDING_FAMILY(JOB_ID_MORBOL, 4, 5, 7),
    BREEDING_FAMILY(JOB_ID_BEHEMOTH, 7, 8, 9),
    BREEDING_FAMILY(JOB_ID_DRAGON, 7, 8, 9),
    BREEDING_FAMILY(JOB_ID_HYUDRA, 9, 9, 9),
};

/*
 * Look up a zodiac value from month/day upper limits.
 *
 * The 13 ordered limits include both Capricorn portions of the year.
 * No date validation is performed; exhausting the 13 limits reads 31 from
 * the following month-length data, not a valid extra zodiac entry.
 */
static s32 birthday_zodiac_get(s32 month, s32 day) {
    s32 i;

    for (i = 0; i < ZODIAC_DATE_LIMIT_COUNT; i++) {
        if (month < s_zodiac_date_limits[i].date.month) {
            break;
        }
        if (month == s_zodiac_date_limits[i].date.month) {
            if (s_zodiac_date_limits[i].date.day >= day) {
                break;
            }
        }
    }
    return s_zodiac_date_limits[i].zodiac;
}

/* Advance egg hatch counters, then make the daily monster-breeding roll.
 *
 * It first reseeds rand from the play time (frames * 101 + seconds * 10 +
 * minutes + hours), so arriving at the same play time repeats the outcome.
 * A counter that reaches 0 hatches as a monster born on the current date.
 * A successful roll selects a random roster monster and one of the three
 * offspring jobs in its Chocobo-based breeding-table entry. The new egg's
 * hatch range comes from its offspring entry, and its level is based on a
 * random non-egg roster member. */
void breeding_update(s32 unused) {
    u8 monsters[0x18];
    u8 levels[0x18];
    s32 monster_count = 0;
    s32 unit_count = 0;
    s32 i = 0;
    /* The original reused two scalar temporaries throughout; splitting them
     * into typed locals changes the callee-saved allocation (n holds the
     * egg counter, month index, rarity class, offspring job and new slot;
     * value holds the birthday, the random draw and the saved name-buffer
     * pointer). */
    s32 value;
    s32 n;
    s32 day;
    s32 month;
    u8** name_buffer;
    party_unit_t* unit;

    srand(g_save_play_time_frames * 100 + g_save_play_time_seconds * 10 + g_save_play_time_minutes
        + g_save_play_time_hours + g_save_play_time_frames);
    do {
        unit = party_unit_get(i);
        if (unit->party_id != PARTY_ID_NONE) {
            if (unit->gender_flags & UNIT_FLAG_EGG) {
                n = unit->birthday_day & 0xF;
                n -= 1;
                unit->birthday_day = (unit->birthday_day & 0xF0) | n;
                if (n == 0) {
                    unit->gender_flags = UNIT_FLAG_MONSTER;
                    day = script_variable_get(EVENT_SCRIPT_VAR_DAY);
                    month = script_variable_get(EVENT_SCRIPT_VAR_MONTH);
                    value = 0;
                    for (n = 1; n < month; n++) {
                        value += s_breeding_month_lengths[n];
                    }
                    value += day;
                    value |= birthday_zodiac_get(month, day) << 12;
                    unit->birthday_day = value;
                    unit->zodiac = value >> 8;
                }
            } else {
                levels[unit_count++] = unit->level;
                value = job_skillset_get(unit->job_id);
                if (value >= SKILLSET_ID_MONSTER_FIRST && value < SKILLSET_ID_END) {
                    monsters[monster_count++] = i;
                }
            }
        }
        i++;
    } while (i < PARTY_ROSTER_SLOT_COUNT);

    if (monster_count == 0) {
        return;
    }
    /* The day's roll out of 256: below 6 rare, below 16 uncommon, below 32 common, else no egg. */
    value = rand() & 0xFF;
    if (value < 6) {
        n = BREEDING_RARITY_RARE;
    } else if (value < 0x10) {
        n = BREEDING_RARITY_UNCOMMON;
    } else {
        n = value < 0x20; /* COMMON or NONE */
    }
    if (n == BREEDING_RARITY_NONE) {
        return;
    }
    monster_count = rand() % monster_count;
    n = s_breeding_table[party_unit_get(monsters[monster_count])->job_id - JOB_ID_CHOCOBO]
            .offspring_job_by_rarity[n - BREEDING_RARITY_COMMON];
    i = rand() % s_breeding_table[n - JOB_ID_CHOCOBO].hatch_range + 1;
    if (i < 2) {
        i = 2;
    }
    /* The egg's hatch count goes in both nibbles; the low one counts down. Name generation
     * reads the unit-name table through the roster-name pointer, swapped for the call as
     * formation_generate_unit does. */
    name_buffer = &g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    value = (s32)*name_buffer;
    *name_buffer = g_text_section_pointers[TEXT_SECTION_UNIT_NAMES];
    n = party_monster_egg_create(n, i + i * 0x10, rand() % 4);
    *name_buffer = (u8*)value;
    if (n != -1) {
        unit = party_unit_get(n);
        party_levels_gain(unit, levels[rand() % unit_count] - 1);
    }
}

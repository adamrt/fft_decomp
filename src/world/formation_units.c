/* Formation changes to a unit's abilities and learned skills, with roster and
 * sprite lookup helpers used by those menus. The final routines translate
 * birthdays to zodiac signs and advance monster eggs as the calendar changes. */

#include "fft/ability.h"
#include "fft/calendar.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/formation_item.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/party.h"
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/skillset.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/zodiac.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/stddef.h"
#include "psx/types.h"

/* One inclusive date limit and its zodiac ID. The 13-entry WORLD table has
 * Capricorn at both calendar ends and does not contain Serpentarius. */
typedef struct zodiac_date_limit {
    calendar_date_t date;
    u8 zodiac;
} zodiac_date_limit_t;
typedef char zodiac_date_limit_size_must_be_3[(sizeof(zodiac_date_limit_t) == 3) ? 1 : -1];

enum { ZODIAC_DATE_LIMIT_COUNT = 13 };

typedef enum breeding_rarity {
    BREEDING_RARITY_NONE = 0,
    BREEDING_RARITY_COMMON = 1,
    BREEDING_RARITY_UNCOMMON = 2,
    BREEDING_RARITY_RARE = 3
} breeding_rarity_e;

enum {
    BREEDING_RARITY_COUNT = 3,
    BREEDING_MONTH_COUNT = 13,
    BREEDING_ENTRY_COUNT = JOB_ID_TIAMAT - JOB_ID_CHOCOBO + 1
};

/* Breeding data for one monster job: its hatch-day range and offspring jobs
 * for the common, uncommon and rare rolls. */
typedef struct breeding_entry {
    u8 hatch_range;                                    /* 0x00: rand() % hatch_range + 1 (at least 2) egg days */
    u8 offspring_job_by_rarity[BREEDING_RARITY_COUNT]; /* 0x01; common through rare */
} breeding_entry_t;
typedef char breeding_entry_size_must_be_4[(sizeof(breeding_entry_t) == 4) ? 1 : -1];

/* sprite sources, zodiac dates and breeding tables */
static const zodiac_date_limit_t g_zodiac_date_limits[ZODIAC_DATE_LIMIT_COUNT] = {
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
extern texture_source_t g_formation_sprite_sources[];

/* Fixed character identities select sprite sources here; jobs below 0x35 use the same mapping. */
extern u8 g_formation_sprite_index_by_identity[];

/* Days per month, indexed from 1; the leading zero is outside the sum. */
static const u8 g_breeding_month_lengths[BREEDING_MONTH_COUNT] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

/* Each three-job monster family shares common/uncommon offspring. The first
 * job's rare result stays at that job; the other two use the third job. */
#define BREEDING_FAMILY(job, first_range, second_range, third_range)                                                   \
    { first_range, { job, job + 1, job } }, { second_range, { job, job + 1, job + 2 } }, {                             \
        third_range, { job, job + 1, job + 2 }                                                                         \
    }
static const breeding_entry_t g_breeding_table[BREEDING_ENTRY_COUNT] = {
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
#undef BREEDING_FAMILY

/*
 * Change an ability slot and optionally replace incompatible equipment.
 *
 * The record is saved to the party and anything the new ability set no
 * longer allows is removed. With `update_equipment`, a unit other than the
 * preview unit and not a Mime is then given its best-fit equipment: the old
 * items return to the inventory before the replacement set is taken from it,
 * and the auto-equip banner (0xD811) shows for 30 frames. Return 1 on success
 * and -2 when the unit is locked (see formation_get_unit_lock).
 */
s32 formation_ability_set_slot(s16 unit_id, s16 slot, s16 ability, s32 update_equipment) {
    s16 equipment[MENU_EQUIPMENT_SLOT_COUNT];
    s32 i;

    if (formation_get_unit_lock(unit_id) == FORMATION_UNIT_UNLOCKED) {
        formation_unit_t* unit = g_formation_unit_pointers[unit_id];
        unit->detail.abilities[slot] = ability;
        formation_save_records();
        formation_remove_invalid_loadout(unit_id);
        if (update_equipment != 0 && unit_id != FORMATION_PREVIEW_UNIT_INDEX
            && g_formation_unit_pointers[unit_id]->identity.job_id != JOB_ID_MIME) {
            formation_item_build_best_fit(unit_id, equipment);
            for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                item_adjust_inventory_count(g_formation_unit_pointers[unit_id]->detail.equipment[i], 1);
            }
            for (i = 0; i < MENU_EQUIPMENT_SLOT_COUNT; i++) {
                g_formation_unit_pointers[unit_id]->detail.equipment[i] = equipment[i];
                item_adjust_inventory_count(equipment[i], -1);
            }
            formation_save_records();
            formation_banner_countdown(0xD811, 0x1E);
        }
        return 1;
    }
    return -2;
}

/* Why a formation unit cannot use a menu, as a FORMATION_UNIT_LOCK_*
 * value: away on a proposition, a monster, an egg or a Mime, tested in that
 * order; FORMATION_UNIT_UNLOCKED otherwise. */
s32 formation_get_unit_lock(s32 unit_id) {
    formation_unit_t* unit = g_formation_unit_pointers[unit_id];

    if (unit->proposition_status != 0)
        return FORMATION_UNIT_LOCK_PROPOSITION;
    if (unit->detail.uses_monster_skillset != 0)
        return FORMATION_UNIT_LOCK_MONSTER;
    if (job_is_special_monster(unit->identity.job_id) != 0)
        return FORMATION_UNIT_LOCK_MONSTER;
    if (g_formation_unit_pointers[unit_id]->gender_flags & UNIT_FLAG_EGG)
        return FORMATION_UNIT_LOCK_EGG;
    if (g_formation_unit_pointers[unit_id]->identity.job_id == JOB_ID_MIME)
        return FORMATION_UNIT_LOCK_MIME;
    return FORMATION_UNIT_UNLOCKED;
}

/* Mark an ability learned for a job: find its position in the job's skillset list and set that
 * bit, most significant first, in the unit's learned-ability bytes for the job. */
void formation_ability_learn(s16 unit_id, s16 job_id, s16 ability_id) {
    s32 skillset = job_get_skillset(job_id);
    s16 generic_job = job_get_generic_index(job_id);
    s16* list = (s16*)ability_store_skillset_abilities(skillset, SKILLSET_ABILITY_FILTER_ALL);
    s32 i = 0;
    s32 byte;
    s32 bit;
    s32 mask;
    u8* learned_abilities;

    while (list[i] != ability_id) {
        i++;
    }
    byte = i >> 3;
    bit = 7 - (i & 7);
    mask = 1;
    i = mask << bit;
    /* Keeping the job-byte stride before the field address preserves the retail load/add order. */
    learned_abilities = (u8*)g_formation_unit_pointers[unit_id]
        + generic_job * sizeof(g_formation_unit_pointers[unit_id]->learned_abilities[0])
        + offsetof(formation_unit_t, learned_abilities);
    learned_abilities[byte] |= i;
}

/* Build the sprite texture source and palette for a formation unit.
 *
 * Special jobs use fixed sprite entries; generic humans derive theirs from
 * job and gender. The third argument is retained because all known callers
 * pass it even though this function does not consume it. */
void formation_build_unit_sprite_source(s16 formation_index, texture_source_t* destination, s32 unused) {
    formation_unit_t* unit = g_formation_unit_pointers[formation_index];
    s16 job_id;
    s32 index;
    s32 palette;
    s16 clut_x;

    job_id = unit->identity.job_id;
    if (job_id == JOB_ID_BYBLOS) {
        index = 0x4E;
    } else if (job_id == JOB_ID_STEEL_GIANT) {
        index = 0x52;
    } else if (job_id == JOB_ID_APANDA) {
        index = 0x4E;
    } else if (job_id == JOB_ID_SERPENTARIUS) {
        index = 0x4F;
    } else if (job_id == JOB_ID_ARCHAIC_DEMON) {
        index = 0x51;
    } else if (job_id == JOB_ID_ULTIMA_DEMON) {
        index = 0x51;
    } else if (job_id == JOB_ID_HOLY_DRAGON_REIS) {
        index = 0x4C;
    } else if (unit->sprite_set == CHARACTER_IDENTITY_MONSTER) {
        index = unit->monster_graphic + 0x3D;
    } else if (unit->sprite_set < CHARACTER_IDENTITY_BOY) {
        index = g_formation_sprite_index_by_identity[unit->sprite_set];
    } else {
        formation_build_job_sprite_source(job_id, unit->gender_flags & UNIT_FLAG_FEMALE, destination);
        return;
    }
    bcopy(&g_formation_sprite_sources[index], destination, sizeof(*destination));
    palette = (s16)formation_get_unit_palette_index(g_formation_unit_pointers[formation_index]->identity.roster_slot);
    clut_x = (palette % 4) * 0x10 + 0x140;
    job_id = palette / 4;
    destination->clut = GetClut(clut_x, job_id + 0xE0);
}

/* Find the current formation-list index for a roster slot.
 *
 * Narrow the requested slot to s16. On a miss, return the number of entries searched.
 */
s32 formation_find_by_roster_slot(s16 roster_slot) {
    s32 index;

    for (index = 0; index < g_formation_record_count; index++) {
        if (g_formation_unit_pointers[index]->identity.roster_slot == roster_slot) {
            break;
        }
    }
    return index;
}

/* Build the sprite texture source for a job. Jobs below 0x35 share their
 * character identity lookup; of the rest, Bard and Dancer have fixed entries
 * (0x3a, 0x3b), while Mime uses 0x3c and the others job * 2 - 0x7c, both
 * plus one for a female unit. */
void formation_build_job_sprite_source(s32 job_id, s16 is_female, texture_source_t* destination) {
    s16 id = job_id;
    s16 idx;

    if (id < JOB_ID_CHEMIST_35) {
        idx = g_formation_sprite_index_by_identity[id];
    } else if (id == JOB_ID_DANCER) {
        idx = 0x3B;
    } else if (id == JOB_ID_BARD) {
        idx = 0x3A;
    } else {
        if (id == JOB_ID_MIME) {
            idx = 0x3C;
        } else {
            idx = (job_id * 2) - 0x7C;
        }
        if (is_female != 0) {
            idx += 1;
        }
    }
    bcopy(&g_formation_sprite_sources[idx], destination, sizeof(*destination));
}

/*
 * Copy a formation unit into the shared temporary record.
 *
 * The snapshot caller supplies the preview index as a second argument; this
 * routine ignores it and always uses g_formation_temp_unit.
 */
void formation_copy_unit_to_temp(s16 index, s32 unused_slot) {
    bcopy(g_formation_unit_pointers[index], g_formation_temp_unit, sizeof(formation_unit_t));
}

/*
 * Look up a zodiac value from month/day upper limits.
 *
 * The 13 ordered limits include both Capricorn portions of the year.
 * No date validation is performed; exhausting the 13 limits reads 31 from
 * the following month-length data, not a valid extra zodiac entry.
 */
static s32 zodiac_get_for_date(s32 month, s32 day) {
    s32 i;

    for (i = 0; i < ZODIAC_DATE_LIMIT_COUNT; i++) {
        if (month < g_zodiac_date_limits[i].date.month) {
            break;
        }
        if (month == g_zodiac_date_limits[i].date.month) {
            if (g_zodiac_date_limits[i].date.day >= day) {
                break;
            }
        }
    }
    return g_zodiac_date_limits[i].zodiac;
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
        unit = party_get_unit(i);
        if (unit->party_id != PARTY_ID_NONE) {
            if (unit->gender_flags & UNIT_FLAG_EGG) {
                n = unit->birthday_day & 0xF;
                n -= 1;
                unit->birthday_day = (unit->birthday_day & 0xF0) | n;
                if (n == 0) {
                    unit->gender_flags = UNIT_FLAG_MONSTER;
                    day = script_get_variable(EVENT_SCRIPT_VAR_DAY);
                    month = script_get_variable(EVENT_SCRIPT_VAR_MONTH);
                    value = 0;
                    for (n = 1; n < month; n++) {
                        value += g_breeding_month_lengths[n];
                    }
                    value += day;
                    value |= zodiac_get_for_date(month, day) << 12;
                    unit->birthday_day = value;
                    unit->zodiac = value >> 8;
                }
            } else {
                levels[unit_count++] = unit->level;
                value = job_get_skillset(unit->job_id);
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
    n = g_breeding_table[party_get_unit(monsters[monster_count])->job_id - JOB_ID_CHOCOBO]
            .offspring_job_by_rarity[n - BREEDING_RARITY_COMMON];
    i = rand() % g_breeding_table[n - JOB_ID_CHOCOBO].hatch_range + 1;
    if (i < 2) {
        i = 2;
    }
    /* The egg's hatch count goes in both nibbles; the low one counts down. Name generation
     * reads the unit-name table through the roster-name pointer, swapped for the call as
     * formation_generate_unit does. */
    name_buffer = &g_text_section_pointers[TEXT_SECTION_ROSTER_UNIT_NAMES];
    value = (s32)*name_buffer;
    *name_buffer = g_text_section_pointers[TEXT_SECTION_UNIT_NAMES];
    n = party_create_monster_egg(n, i + i * 0x10, rand() % 4);
    *name_buffer = (u8*)value;
    if (n != -1) {
        unit = party_get_unit(n);
        party_level_unit_to_target(unit, levels[rand() % unit_count] - 1);
    }
}

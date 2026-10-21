/* The resident party roster and new-recruit generation. Roster records retain the packed
 * stats, names and job history that survive battles and program loads. */

#include "fft/party.h"
#include "fft/encounter.h"
#include "fft/job.h"
#include "fft/memory.h"
#include "fft/screen.h"
#include "fft/text.h"
#include "fft/unit.h"
#include "fft/unit_entd.h"
#include "fft/unit_identity.h"
#include "fft/unit_party.h"
#include "fft/zodiac.h"
#include "psx/libc.h"
#include "psx/types.h"

/* One row of g_unit_generation_base_data, selected by
 * unit_generation_type_e. */
typedef struct unit_generation_base_data {
    u8 raw_stat_seeds[UNIT_RAW_STAT_COUNT];
    u8 equipment[UNIT_EQUIPMENT_SLOT_COUNT];
} unit_generation_base_data_t;
typedef char unit_generation_base_data_size_must_be_12[(sizeof(unit_generation_base_data_t) == 12) ? 1 : -1];

extern unit_generation_base_data_t g_unit_generation_base_data[UNIT_TYPE_COUNT];
extern u8 g_unit_generation_raw_stat_variance[UNIT_TYPE_COUNT][UNIT_RAW_STAT_COUNT];
static void base_stats_generate(unit_raw_stat_t raw_stats[UNIT_RAW_STAT_COUNT], unit_generation_type_e unit_type);
static void party_record_generate(party_unit_t* party, unit_generation_type_e unit_type);
static void party_base_stats_generate(party_unit_t* party, unit_generation_type_e unit_type);

/* Uncalled empty routine (jr ra; nop in the original) preceding party_clear. */
void party_noop_1(void) {
}

/* Mark all 20 roster slots empty (party_id = PARTY_ID_NONE); the rest of each record is left as
 * is. */
void party_clear(void) {
    s32 i;

    for (i = PARTY_ROSTER_SLOT_COUNT - 1; i >= 0; i--) {
        g_party_units[i].party_id = PARTY_ID_NONE;
    }
}

/* Return the roster record at party_index, or NULL at or past PARTY_ROSTER_SLOT_COUNT (negative
 * indexes are not rejected). */
party_unit_t* party_unit_get(s32 party_index) {
    return party_index < PARTY_ROSTER_SLOT_COUNT ? &g_party_units[party_index] : 0;
}

/* Build a battle unit from one ENTD unit slot on the stack and save it into the
 * party roster. Returns -2 for an empty ENTD slot, otherwise
 * party_unit_save's result (0, or -1 when the roster is full). */
s32 party_encounter_unit_save(const entd_encounter_t* entd, s32 unit_id, s32 allow_guest, s32 use_world_text) {
    const entd_unit_t* entd_unit = &entd->units[unit_id];

    g_encounter_current_unit = entd_unit;
    if (entd_unit->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
        return -2;
    }
    {
        unit_stats_t unit;
        unit_stats_t* unit_data = &unit;

        unit_data->battle_id = 0xff;
        /* Placeholder for this temporary roster-conversion record. */
        unit_data->active_battle_id = 0x20;
        unit_data->existence = UNIT_EXISTENCE_ACTIVE;
        unit_data->formation_index = UNIT_FORMATION_INDEX_NONE;
        unit_encounter_init(unit_data, entd_unit, use_world_text, UNIT_ENTD_INIT_NEW);
        return party_unit_save(unit_data, allow_guest);
    }
}

/*
 * Save an in-battle unit into the party roster. Returns 0, or -1 when there
 * is no room. A unit already in the roster keeps its slot, except that a
 * non-guest save moves a unit out of a guest slot (16-19) into a free
 * ordinary slot. A new unit takes a free ordinary slot, or a free guest slot
 * when allow_guest is set and it is a story character carrying
 * UNIT_FLAG_SAVE_FORMATION. A guest record keeps the unit's palette; an
 * ordinary one gets palette 0 and drops the save-formation flag.
 */
s32 party_unit_save(const unit_stats_t* unit, s32 allow_guest) {
    party_unit_t* party;
    s32 slot;
    s32 index;
    u32 unit_slot;
    u8 kind = unit->unit_flags & UNIT_FLAG_SAVE_FORMATION;
    u8 palette;
    u16 birthday;
    s32 sprite_set;

    if (unit->character_identity >= CHARACTER_IDENTITY_SELECTOR_FIRST || allow_guest == 0) {
        kind = 0;
    }
    index = unit->formation_index;
    unit_slot = index & 0xff;

    if (kind == 0 && index >= PARTY_GUEST_SLOT_FIRST && index < PARTY_ROSTER_SLOT_COUNT) {
        /* A non-guest save of a unit that currently sits in a guest slot. */
        party_unit_remove(index);
        index = party_free_slot_find(0, &palette);
        if (index == -1) {
            return -1;
        }
        slot = index;
    } else if (unit_slot >= UNIT_SLOT_COUNT) {
        /* GCC cross-jumps this search into the identical one above. */
        index = party_free_slot_find(kind, &palette);
        if (index == -1) {
            return -1;
        }
        slot = index;
    } else {
        party_free_slot_find(0, &palette);
        slot = unit_slot;
    }

    party = party_unit_get(slot);
    sprite_set = unit->character_identity;
    party->party_id = slot;
    party->sprite_set = sprite_set;
    party->job_id = unit->job_id;
    if (slot >= PARTY_GUEST_SLOT_FIRST) {
        party->palette = unit->sprite_palette;
        party->gender_flags = unit->unit_flags & ~UNIT_FLAG_EGG;
    } else {
        party->palette = palette;
        party->gender_flags = unit->unit_flags & ~(UNIT_FLAG_EGG | UNIT_FLAG_SAVE_FORMATION);
    }

    /* The 16-bit value at 0x008 packs the birthday (9 bits) and zodiac (top nibble). */
    birthday = unit->birthday.birthday | (unit->birthday.zodiac << 12);
    party->birthday_day = birthday;
    party->zodiac = birthday >> 8;
    mem_copy_byte_data_source_first(&unit->secondary_skillset, &party->secondary_skillset, 0x10);
    party->brave = unit->original_brave;
    party->faith = unit->original_faith;
    mem_copy_byte_data_source_first(unit->raw_stats, party->raw_stats, sizeof(party->raw_stats));
    /* The saved span includes the career record and following 16-byte name. */
    mem_copy_byte_data_source_first(&unit->career, &party->career, sizeof(party->career) + sizeof(party->name));
    /* name_id is stored as two byte writes (lbu / lhu+srl), not one sh. */
    party->name_id.index = unit->quote_name_id.value;
    party->name_id.name_class = unit->quote_name_id.value >> 8;
    party->proposition_status = 0;
    party->egg_color = 0;
    return 0;
}

/* Return the first empty roster slot, or -1 when the searched range is full.
 *
 * A nonzero save_formation searches the four guest slots (16..19), zero the
 * sixteen ordinary slots. *palette is always set to 0. */
s32 party_free_slot_find(s32 save_formation, u8* palette) {
    party_unit_t* party_units[PARTY_ROSTER_SLOT_COUNT];
    s32 party_index;
    s32 start_index;
    s32 limit;

    *palette = 0;

    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_units[party_index] = party_unit_get(party_index);
    }

    if (save_formation != 0) {
        start_index = PARTY_GUEST_SLOT_FIRST;
        limit = PARTY_ROSTER_SLOT_COUNT;
    } else {
        start_index = 0;
        limit = PARTY_MEMBER_SLOT_COUNT;
    }

    for (party_index = start_index; party_index < limit; party_index++) {
        if (party_units[party_index]->party_id == PARTY_ID_NONE) {
            return party_index;
        }
    }

    return -1;
}

/* Add an egg of monster_job to the first empty ordinary roster slot. Returns the
 * slot, or -1 when there is none.
 *
 * egg_modifier is stored as the unit's birthday (values from 0x16e up become 1)
 * and the zodiac sign is derived from it; the breeding code passes the same value
 * in both nibbles. */
s32 party_monster_egg_create(s32 monster_job, s32 egg_modifier, s32 egg_color) {
    s32 party_index = party_unit_generate(UNIT_TYPE_MONSTER);
    party_unit_t* party;
    u16 birthday;
    s32 zodiac;

    if (party_index == -1) {
        return party_index;
    }
    party = party_unit_get(party_index);
    party->job_id = monster_job;
    party->egg_color = egg_color;
    party->gender_flags |= UNIT_FLAG_EGG;
    birthday = egg_modifier;
    if (birthday >= 0x16e) {
        birthday = 1;
    }
    zodiac = zodiac_sign_calculate(birthday);
    party->birthday_day = birthday;
    party->zodiac = ((birthday & 0x100) >> 8) + (zodiac << 4);
    return party_index;
}

/* Generate a new unit of unit_type (UNIT_TYPE_*) in the first empty ordinary
 * roster slot (0..15), with palette 0. Returns the slot, or -1 when all sixteen
 * are taken. */
s32 party_unit_generate(unit_generation_type_e unit_type) {
    party_unit_t* party_units[PARTY_ROSTER_SLOT_COUNT];
    s32 party_index;
    s32 palette = 0;

    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_units[party_index] = party_unit_get(party_index);
    }

    for (party_index = 0; party_index < PARTY_MEMBER_SLOT_COUNT; party_index++) {
        if (party_units[party_index]->party_id == PARTY_ID_NONE) {
            party_units[party_index]->party_id = party_index;
            party_units[party_index]->palette = palette;
            party_record_generate(party_units[party_index], unit_type);
            return party_index;
        }
    }

    return -1;
}

/* Return the first occupied roster slot whose sprite set matches, or -1. */
s32 party_sprite_slot_find(s32 sprite_set) {
    s32 party_index = 0;
    s32 empty_party_id = PARTY_ID_NONE;
    party_unit_t* party = g_party_units;

    for (; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++, party++) {
        if (party->party_id != empty_party_id && party->sprite_set == sprite_set) {
            return party_index;
        }
    }

    return -1;
}

/* Free a roster slot by marking its party_id empty; the record's other bytes are kept. */
void party_unit_remove(u32 index) {
    g_party_units[index].party_id = PARTY_ID_NONE;
}

/* Fills a fresh party record for a generic male (0), generic female (1),
 * monster (3) or Ramza (anything else, forced to 2).
 *
 * The birthday is a random day 1..365. Brave and Faith are each rolled in
 * 40..70; Ramza gets 70 and 70. The unit starts at level 1 with 0 EXP, and a
 * human gets 100..199 JP and job level 1 in every job but the one its sex
 * cannot take (Bard, Dancer). The name is drawn until no party member of the
 * same kind has it; the roll rand() * 255 / 0x8000 gives 0..254, so name
 * index 255 is never drawn. */
static void party_record_generate(party_unit_t* party, unit_generation_type_e unit_type) {
    s32 name_flags;
    unit_name_id_t name_base;
    unit_name_id_t name_id;
    unit_name_id_t existing_name;
    u16 birthday;
    s32 zodiac;
    s32 gender;
    s32 i;
    u8 generic_id;
    unit_generation_base_data_t* base;
    unit_generation_base_data_t* table;
    party_unit_t* other;
    s32 found;
    s32 jp;

    if (unit_type == UNIT_TYPE_MALE) {
        name_flags = TEXT_ID_UNIT_NAME_GENERIC_MALE_BASE;
        name_base.value = 0;
        name_base.fields.name_class = PARTY_NAME_CLASS_GENERIC_MALE;
        party->sprite_set = CHARACTER_IDENTITY_GENERIC_MALE;
        party->gender_flags = UNIT_FLAG_MALE;
        party->job_id = JOB_ID_SQUIRE;
    } else if (unit_type == UNIT_TYPE_FEMALE) {
        name_flags = TEXT_ID_UNIT_NAME_GENERIC_FEMALE_BASE;
        name_base.value = 0;
        name_base.fields.name_class = PARTY_NAME_CLASS_GENERIC_FEMALE;
        party->sprite_set = CHARACTER_IDENTITY_GENERIC_FEMALE;
        party->gender_flags = UNIT_FLAG_FEMALE;
        party->job_id = JOB_ID_SQUIRE;
    } else if (unit_type == UNIT_TYPE_MONSTER) {
        name_flags = TEXT_ID_UNIT_NAME_GENERIC_MONSTER_BASE;
        name_base.value = 0;
        name_base.fields.name_class = PARTY_NAME_CLASS_GENERIC_MONSTER;
        party->sprite_set = CHARACTER_IDENTITY_MONSTER;
        party->gender_flags = UNIT_FLAG_MONSTER;
        party->job_id = JOB_ID_SQUIRE;
    } else {
        name_flags = TEXT_ID_UNIT_NAME_SPECIAL_BASE;
        unit_type = UNIT_TYPE_RAMZA;
        name_base.value = 0;
        name_base.fields.name_class = PARTY_NAME_CLASS_SPECIAL;
        party->sprite_set = CHARACTER_IDENTITY_RAMZA_CHAPTER_1;
        party->gender_flags = UNIT_FLAG_MALE;
        party->job_id = JOB_ID_SQUIRE_RAMZA_CHAPTER_1;
    }

    birthday = (rand() * 365) / 0x8000 + 1;
    zodiac = zodiac_sign_calculate(birthday) << PARTY_ZODIAC_SHIFT;
    party->birthday_day = birthday;
    party->zodiac = ((birthday & (PARTY_BIRTHDAY_DAY_HIGH_BIT_MASK << 8)) >> 8) + zodiac;

    if (unit_type == UNIT_TYPE_RAMZA) {
        party->brave = 70;
        party->faith = 70;
    } else {
        party->brave = (rand() * 31) / 0x8000 + 40;
        party->faith = (rand() * 31) / 0x8000 + 40;
    }

    mem_zero(&party->secondary_skillset, 7);
    party_base_stats_generate(party, unit_type);
    party->level = 1;
    party->experience = 0;
    mem_zero(&party->career, sizeof(party->career));

    gender = party->gender_flags;
    if (gender & (UNIT_FLAG_FEMALE | UNIT_FLAG_MALE)) {
        for (i = 0; i < UNIT_CAREER_JOB_COUNT; i++) {
            if (i == UNIT_CAREER_JOB_INDEX_BARD && (gender & UNIT_FLAG_FEMALE)) {
                continue;
            }
            if (i == UNIT_CAREER_JOB_INDEX_DANCER && (gender & UNIT_FLAG_MALE)) {
                continue;
            }
            jp = (rand() * 100) / 0x8000 + 100;
            party->career.job_points.total.entries[i].low = jp;
            party->career.job_points.current.entries[i].low = jp;
            party->career.job_levels[i / UNIT_JOBS_PER_LEVEL_BYTE] = UNIT_JOB_LEVEL_ONE_BOTH_NIBBLES;
        }
    }
    if (gender & UNIT_FLAG_MALE) {
        party->career.job_levels[UNIT_JOB_LEVEL_BYTE_INDEX_DANCER_MIME] = UNIT_JOB_LEVEL_ONE_LOW_NIBBLE;
    }
    if (gender & UNIT_FLAG_FEMALE) {
        party->career.job_levels[UNIT_JOB_LEVEL_BYTE_INDEX_CALCULATOR_BARD] = UNIT_JOB_LEVEL_ONE_HIGH_NIBBLE;
    }

    table = g_unit_generation_base_data;
    base = table + unit_type;
    generic_id = party->sprite_set;
    party->career.unlocked_jobs[0] = UNIT_UNLOCKED_JOB_SQUIRE >> 16;
    /* Keep sprite sets 0x80+ for the name-uniqueness comparison; collapse
     * other identities to zero. */
    generic_id = generic_id >= CHARACTER_IDENTITY_SELECTOR_FIRST ? generic_id : 0;
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        party->equipment[i] = base->equipment[i];
    }

    if (unit_type == UNIT_TYPE_RAMZA) {
        name_id.value = 1;
    } else {
        party->name_id.index = 0xff;
        party->name_id.name_class = name_base.fields.name_class;
        do {
            found = 1;
            name_id.value = name_base.value + (rand() * 255) / 0x8000;
            for (i = 0; i < PARTY_MEMBER_SLOT_COUNT; i++) {
                other = &g_party_units[i];
                if (other->party_id != PARTY_ID_NONE && (other->sprite_set & -(other->sprite_set >> 7)) == generic_id) {
                    existing_name.value = 0;
                    existing_name.fields.name_class = other->name_id.name_class;
                    existing_name.fields.index = other->name_id.index;
                    if (existing_name.value == name_id.value) {
                        found = 0;
                        break;
                    }
                }
            }
        } while (found == 0);
    }
    party->name_id.index = name_id.fields.index;
    party->name_id.name_class = name_id.fields.name_class;
    name_flags += name_id.fields.index;
    mem_copy_bytes_source_first(world_text_find_entry(name_flags), party->name, sizeof(party->name));
    party->proposition_status = 0;
    party->egg_color = 0;
    if (unit_type == UNIT_TYPE_RAMZA) {
        party->career.learned_abilities[0][0] = 4;
    }
}

/* The target leaves a1 intact while advancing a0 to the raw-stat field. */
static void party_base_stats_generate(party_unit_t* party, unit_generation_type_e unit_type) {
    base_stats_generate(party->raw_stats, unit_type);
}

/* Roll the unit's raw stats by its sex flags; a unit with neither flag is rolled
 * as a monster. */
void unit_raw_stats_generate(unit_stats_t* unit) {
    unit_generation_type_e unit_type;

    if ((unit->unit_flags & UNIT_FLAG_MALE) != 0) {
        unit_type = UNIT_TYPE_MALE;
    } else if ((unit->unit_flags & UNIT_FLAG_FEMALE) != 0) {
        unit_type = UNIT_TYPE_FEMALE;
    } else {
        unit_type = UNIT_TYPE_MONSTER;
    }
    base_stats_generate(unit->raw_stats, unit_type);
}

/* Roll the five little-endian 24-bit raw stats for the selected unit type. */
static void base_stats_generate(unit_raw_stat_t raw_stats[UNIT_RAW_STAT_COUNT], unit_generation_type_e unit_type) {
    /* Without the pin, type_index, index and base rotate among $s2/$s3/$s4. */
    register s32 type_index __asm__("$20") = unit_type;
    s32 index = 0;
    u8(*variance)[UNIT_RAW_STAT_COUNT] = g_unit_generation_raw_stat_variance;
    unit_generation_base_data_t* table = g_unit_generation_base_data;
    u8* base = table[type_index].raw_stat_seeds;

    do {
        s32 value = base[index] << 14;

        /* Removing the tied use changes the prologue schedule. */
        __asm__("" : "=r"(type_index) : "0"(type_index));
        value += (rand() * variance[type_index][index]) / 2;
        raw_stats[index].low = value;
        raw_stats[index].middle = (u32)value >> 8;
        raw_stats[index].high = value >> 16;
        index++;
    } while (index < UNIT_RAW_STAT_COUNT);
}

/* Copy the current job's growth and multiplier pairs for HP, MP, Speed, PA and MA into the unit. */
void unit_job_growths_copy(unit_stats_t* unit) {
    mem_copy_byte_data_source_first(
        g_job_data_pointer[unit->job_id].stat_growths, unit->stat_growths, sizeof(unit->stat_growths));
}

#include "fft/main.h"
#include "fft/memory.h"
#include "fft/world.h"

/* Fills a fresh party record for a generic male (0), generic female (1),
 * monster (3) or Ramza (anything else, forced to 2).
 *
 * The birthday is a random day 1..365. Brave and Faith are each rolled in
 * 40..70; Ramza gets 70 and 70. The unit starts at level 1 with 0 EXP, and a
 * human gets 100..199 JP and job level 1 in every job but the one its sex
 * cannot take (Bard, Dancer). The name is drawn until no party member of the
 * same kind has it. */
void main_unit_generate_out_of_battle(party_unit_t* party, main_unit_generation_type_e unit_type) {
    s32 name_flags;
    unit_name_id_t name_base;
    unit_name_id_t name_id;
    unit_name_id_t existing_name;
    u16 birthday;
    s32 zodiac;
    s32 gender;
    s32 i;
    u8 generic_id;
    main_unit_generation_base_data_t* base;
    main_unit_generation_base_data_t* table;
    party_unit_t* other;
    s32 found;
    s32 jp;

    if (unit_type == MAIN_UNIT_TYPE_MALE) {
        name_flags = TEXT_ID_UNIT_NAME_GENERIC_MALE_BASE;
        name_base.value = 0;
        name_base.fields.name_class = PARTY_NAME_CLASS_GENERIC_MALE;
        party->sprite_set = CHARACTER_IDENTITY_GENERIC_MALE;
        party->gender_flags = UNIT_FLAG_MALE;
        party->job_id = JOB_ID_SQUIRE;
    } else if (unit_type == MAIN_UNIT_TYPE_FEMALE) {
        name_flags = TEXT_ID_UNIT_NAME_GENERIC_FEMALE_BASE;
        name_base.value = 0;
        name_base.fields.name_class = PARTY_NAME_CLASS_GENERIC_FEMALE;
        party->sprite_set = CHARACTER_IDENTITY_GENERIC_FEMALE;
        party->gender_flags = UNIT_FLAG_FEMALE;
        party->job_id = JOB_ID_SQUIRE;
    } else if (unit_type == MAIN_UNIT_TYPE_MONSTER) {
        name_flags = TEXT_ID_UNIT_NAME_GENERIC_MONSTER_BASE;
        name_base.value = 0;
        name_base.fields.name_class = PARTY_NAME_CLASS_GENERIC_MONSTER;
        party->sprite_set = CHARACTER_IDENTITY_MONSTER;
        party->gender_flags = UNIT_FLAG_MONSTER;
        party->job_id = JOB_ID_SQUIRE;
    } else {
        name_flags = TEXT_ID_UNIT_NAME_SPECIAL_BASE;
        unit_type = MAIN_UNIT_TYPE_RAMZA;
        name_base.value = 0;
        name_base.fields.name_class = PARTY_NAME_CLASS_SPECIAL;
        party->sprite_set = CHARACTER_IDENTITY_RAMZA_CHAPTER_1;
        party->gender_flags = UNIT_FLAG_MALE;
        party->job_id = JOB_ID_SQUIRE_RAMZA_CHAPTER_1;
    }

    birthday = (rand() * 365) / 0x8000 + 1;
    zodiac = main_unit_calculate_zodiac_symbol(birthday) << PARTY_ZODIAC_SHIFT;
    party->birthday_day = birthday;
    party->zodiac = ((birthday & (PARTY_BIRTHDAY_DAY_HIGH_BIT_MASK << 8)) >> 8) + zodiac;

    if (unit_type == MAIN_UNIT_TYPE_RAMZA) {
        party->bravery = 70;
        party->faith = 70;
    } else {
        party->bravery = (rand() * 31) / 0x8000 + 40;
        party->faith = (rand() * 31) / 0x8000 + 40;
    }

    mem_zero(&party->secondary_skillset, 7);
    main_unit_generate_party_base_raw_stats(party, unit_type);
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

    table = g_main_unit_generation_base_data;
    base = table + unit_type;
    generic_id = party->sprite_set;
    party->career.unlocked_jobs[0] = UNIT_UNLOCKED_JOB_SQUIRE >> 16;
    /* Keep sprite sets 0x80+ for the name-uniqueness comparison; collapse
     * other identities to zero. */
    generic_id = generic_id >= CHARACTER_IDENTITY_SELECTOR_FIRST ? generic_id : 0;
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        party->equipment[i] = base->equipment[i];
    }

    if (unit_type == MAIN_UNIT_TYPE_RAMZA) {
        name_id.value = 1;
    } else {
        party->name_id.fields.index = 0xff;
        party->name_id.fields.name_class = name_base.fields.name_class;
        do {
            found = 1;
            name_id.value = name_base.value + (rand() * 255) / 0x8000;
            for (i = 0; i < PARTY_MEMBER_SLOT_COUNT; i++) {
                other = &g_main_party_units[i];
                if (other->party_id != PARTY_ID_NONE && (other->sprite_set & -(other->sprite_set >> 7)) == generic_id) {
                    existing_name.value = 0;
                    existing_name.fields.name_class = other->name_id.fields.name_class;
                    existing_name.fields.index = other->name_id.fields.index;
                    if (existing_name.value == name_id.value) {
                        found = 0;
                        break;
                    }
                }
            }
        } while (found == 0);
    }
    party->name_id.fields.index = name_id.fields.index;
    party->name_id.fields.name_class = name_id.fields.name_class;
    name_flags += name_id.fields.index;
    mem_copy_bytes_source_first(world_text_find_entry(name_flags), party->name, sizeof(party->name));
    party->proposition_status = 0;
    party->egg_color = 0;
    if (unit_type == MAIN_UNIT_TYPE_RAMZA) {
        party->career.learned_abilities[0][0] = 4;
    }
}

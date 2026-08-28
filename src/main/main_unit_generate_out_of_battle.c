#include "fft/battle.h"
#include "fft/data.h"
#include "fft/job.h"
#include "fft/main_heap.h"
#include "fft/main_unit.h"
#include "fft/text.h"
#include "fft/world.h"
#include "psx/libc.h"

/* Byte view of party_data_t: the 16-bit name id is read and written one
 * byte at a time by this routine (lbu/sb pairs in the target). */
typedef struct main_party_name_view {
    u8 sprite_set; /* 0x00 */
    u8 party_id;   /* 0x01 */
    u8 _pad02[0xcc];
    u8 name_id_lo; /* 0xce */
    u8 name_id_hi; /* 0xcf */
    u8 _padd0[0x30];
} main_party_name_view_t;
/*
 * Fills a fresh party record for a generic male (0), generic female (1),
 * monster (3) or Ramza (anything else, forced to 2).
 */
void main_unit_generate_out_of_battle(party_data_t* party, s32 unit_type) {
    /* Pin: unpinned, combine turns the text-id addu into an or (disjoint known bits). */
    register s32 name_flags __asm__("$23");
    u32 name_modifier;
    u16 birthday;
    s32 zodiac;
    s32 gender;
    s32 i;
    u32 name_id;
    u8 generic_id;
    main_unit_generation_base_data_t* base;
    main_unit_generation_base_data_t* table;
    main_party_name_view_t* other;
    s32 found;
    s32 jp;
    u32 hi;

    if (unit_type == MAIN_UNIT_TYPE_MALE) {
        name_flags = TEXT_ID_UNIT_NAME_GENERIC_MALE_BASE;
        name_modifier = PARTY_NAME_CLASS_GENERIC_MALE;
        party->sprite_set = CHARACTER_IDENTITY_GENERIC_MALE;
        party->gender_flags = UNIT_FLAG_MALE;
        party->job_id = JOB_ID_SQUIRE;
    } else if (unit_type == MAIN_UNIT_TYPE_FEMALE) {
        name_flags = TEXT_ID_UNIT_NAME_GENERIC_FEMALE_BASE;
        name_modifier = PARTY_NAME_CLASS_GENERIC_FEMALE;
        party->sprite_set = CHARACTER_IDENTITY_GENERIC_FEMALE;
        party->gender_flags = UNIT_FLAG_FEMALE;
        party->job_id = JOB_ID_SQUIRE;
    } else if (unit_type == MAIN_UNIT_TYPE_MONSTER) {
        name_flags = TEXT_ID_UNIT_NAME_GENERIC_MONSTER_BASE;
        name_modifier = PARTY_NAME_CLASS_GENERIC_MONSTER;
        party->sprite_set = CHARACTER_IDENTITY_MONSTER;
        party->gender_flags = UNIT_FLAG_MONSTER;
        party->job_id = JOB_ID_SQUIRE;
    } else {
        name_flags = TEXT_ID_UNIT_NAME_SPECIAL_BASE;
        unit_type = MAIN_UNIT_TYPE_RAMZA;
        name_modifier = PARTY_NAME_CLASS_SPECIAL;
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

    main_util_clear_byte_data(&party->secondary_skillset, 7);
    main_unit_generate_party_base_raw_stats(party, unit_type);
    party->level = 1;
    party->experience = 0;
    main_util_clear_byte_data(party->unlocked_jobs, UNIT_CAREER_DATA_BYTE_COUNT);

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
            party->total_job_points[i * UNIT_JOB_POINT_BYTES_PER_JOB] = jp;
            party->job_points[i * UNIT_JOB_POINT_BYTES_PER_JOB] = jp;
            party->job_levels[i / UNIT_JOBS_PER_LEVEL_BYTE] = UNIT_JOB_LEVEL_ONE_BOTH_NIBBLES;
        }
    }
    if (gender & UNIT_FLAG_MALE) {
        party->job_levels[UNIT_JOB_LEVEL_BYTE_INDEX_DANCER_MIME] = UNIT_JOB_LEVEL_ONE_LOW_NIBBLE;
    }
    if (gender & UNIT_FLAG_FEMALE) {
        party->job_levels[UNIT_JOB_LEVEL_BYTE_INDEX_CALCULATOR_BARD] = UNIT_JOB_LEVEL_ONE_HIGH_NIBBLE;
    }

    table = g_main_unit_generation_base_data;
    base = table + unit_type;
    generic_id = party->sprite_set;
    party->unlocked_jobs[0] = 0x80;
    /* Keep sprite sets 0x80+ for the name-uniqueness comparison; collapse
     * other identities to zero. */
    generic_id &= -(generic_id >> 7);
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        party->equipment[i] = base->equipment[i];
    }

    hi = name_modifier >> PARTY_NAME_CLASS_SHIFT;
    if (unit_type == MAIN_UNIT_TYPE_RAMZA) {
        name_id = 1;
    } else {
        /* volatile keeps the two byte stores ordered as the target has them
         * (the scheduler otherwise sinks the low byte past the high byte). */
        ((volatile main_party_name_view_t*)party)->name_id_lo = 0xff;
        ((volatile main_party_name_view_t*)party)->name_id_hi = hi;
        do {
            found = 1;
            name_id = name_modifier + (rand() * 255) / 0x8000;
            for (i = 0; i < PARTY_GUEST_SLOT_FIRST; i++) {
                other = (main_party_name_view_t*)&g_main_party_data[i];
                if (other->party_id != PARTY_ID_NONE && (other->sprite_set & -(other->sprite_set >> 7)) == generic_id
                    && (other->name_id_lo | (other->name_id_hi << 8)) == (name_id & 0xffff)) {
                    found = 0;
                    break;
                }
            }
        } while (found == 0);
    }
    /* Reuse the earlier arithmetic temporary: a separate single-set value
     * sinks the shift; reusing hi conflicts with v0 and selects v1. */
    zodiac = name_id >> PARTY_NAME_CLASS_SHIFT;
    ((main_party_name_view_t*)party)->name_id_lo = name_id;
    ((main_party_name_view_t*)party)->name_id_hi = zodiac;
    main_util_copy_bytes(world_text_find_entry(name_flags + (name_id & 0xff)), party->name, 0x10);
    party->proposition_status = 0;
    party->egg_color = 0;
    if (unit_type == MAIN_UNIT_TYPE_RAMZA) {
        party->learned_abilities[0] = 4;
    }
}

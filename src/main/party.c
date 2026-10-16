/* Party slots and unit generation, raw stats, skillset abilities, item and job data pointers, job and battle data init
 * from ENTD, RSM flags, actual stats, abilities, equipment, character names and the highest party level. */

#include "fft/battle_text.h"
#include "fft/data.h"
#include "fft/main.h"
#include "fft/main_item.h"
#include "fft/main_unit.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/unit.h"
#include "fft/world_text.h"
#include "psx/libc.h"
#include "psx/types.h"

static s32 main_job_get_random_unlocked(const unit_stats_t* unit);
static void main_unit_apply_equipment_move_jump_and_name(unit_stats_t* unit);
static void main_unit_calculate_abilities(unit_stats_t* unit, entd_unit_t* entd);
static void main_unit_calculate_actual_stats(unit_stats_t* unit, s32 mode);
static s32 main_unit_calculate_entd_data(unit_stats_t* unit, entd_unit_t* entd);
static void main_unit_calculate_entd_equipment(unit_stats_t* unit, entd_unit_t* entd);
static void main_unit_calculate_jobs_and_skillsets_from_entd(unit_stats_t* unit, const entd_unit_t* entd);
static void main_unit_copy_job_data(unit_stats_t* unit);
static void main_unit_generate_base_raw_stats(
    unit_raw_stat_t raw_stats[UNIT_RAW_STAT_COUNT], main_unit_generation_type_e unit_type);
static void main_unit_generate_out_of_battle(party_unit_t* party, main_unit_generation_type_e unit_type);
static void main_unit_generate_party_base_raw_stats(party_unit_t* party, main_unit_generation_type_e unit_type);
static void main_unit_init_battle_data(unit_stats_t* unit, const party_unit_t* party);
static s32 main_unit_init_job_data_from_entd(unit_stats_t* unit, entd_unit_t* entd_unit);
static void main_unit_set_rsm_flag(unit_stats_t* unit, u32 ability_id);
static void main_unit_store_character_names(unit_stats_t* unit);
static void main_unit_store_ramza_name_birthday_zodiac(unit_stats_t* unit);

void main_status_init_check_data(void) {
    int status;
    u8 flags4;
    u8 flags5;
    int byte;
    u8 mask;

    main_mem_zero(g_main_item_location_flags, sizeof(g_main_item_location_flags));
    main_mem_zero(g_main_status_check_sets, sizeof(g_main_status_check_sets));
    for (status = 0; status < STATUS_COUNT; status++) {
        flags4 = g_main_status_effect_data[status].flags_1;
        flags5 = g_main_status_effect_data[status].flags_2;
        byte = status / 8;
        mask = 0x80 >> (status & 7);
        if (flags4 & STATUS_EFFECT_FLAG_1_KO)
            g_main_status_check_sets[MAIN_STATUS_CHECK_SET_KO][byte] |= mask;
        if (flags4 & STATUS_EFFECT_FLAG_1_UNIT_LOST)
            g_main_status_check_sets[MAIN_STATUS_CHECK_SET_UNIT_LOST][byte] |= mask;
        if (flags4 & STATUS_EFFECT_FLAG_1_CHECK_SET_2)
            g_main_status_check_sets[MAIN_STATUS_CHECK_SET_UNREAD_2][byte] |= mask;
        if (flags4 & STATUS_EFFECT_FLAG_1_FREEZE_CT)
            g_main_status_check_sets[MAIN_STATUS_CHECK_SET_FREEZE_CT][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_PREVENT_REACTION)
            g_main_status_check_sets[MAIN_STATUS_CHECK_SET_PREVENT_REACTION][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_IMMORTAL_IMMUNITY)
            g_main_status_check_sets[MAIN_STATUS_CHECK_SET_IMMORTAL_IMMUNITY][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_FORMATION_IMMUNITY)
            g_main_status_check_sets[MAIN_STATUS_CHECK_SET_FORMATION_IMMUNITY][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_CHECK_SET_7)
            g_main_status_check_sets[MAIN_STATUS_CHECK_SET_UNREAD_7][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED)
            g_main_status_check_sets[MAIN_STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_MOUNT_REMOVAL)
            g_main_status_check_sets[MAIN_STATUS_CHECK_SET_MOUNT_REMOVAL][byte] |= mask;
    }
    g_main_status_check_sets[MAIN_STATUS_CHECK_SET_UNMOUNTABLE][STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        = STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD);
    g_main_status_check_sets[MAIN_STATUS_CHECK_SET_UNMOUNTABLE][STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        = STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)
        | STATUS_BYTE_MASK(STATUS_ID_TREASURE);
    g_main_status_check_sets[MAIN_STATUS_CHECK_SET_UNMOUNTABLE][STATUS_BYTE_INDEX(STATUS_ID_BERSERK)]
        = STATUS_BYTE_MASK(STATUS_ID_BERSERK) | STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG);
    g_main_status_check_sets[MAIN_STATUS_CHECK_SET_UNMOUNTABLE][STATUS_BYTE_INDEX(STATUS_ID_CHARM)]
        = STATUS_BYTE_MASK(STATUS_ID_CHARM);
}

/* An empty routine (jr ra; nop in the original) between
 * main_status_init_check_data and main_party_clear_all. No function in the tree
 * calls it. */
void main_noop_80059ac0(void) {
}

void main_party_clear_all(void) {
    s32 i;

    for (i = PARTY_ROSTER_SLOT_COUNT - 1; i >= 0; i--) {
        g_main_party_units[i].party_id = PARTY_ID_NONE;
    }
}

party_unit_t* main_party_get_unit(s32 party_index) {
    return party_index < PARTY_ROSTER_SLOT_COUNT ? &g_main_party_units[party_index] : 0;
}

/* Build a battle unit from one ENTD unit slot on the stack and save it into the
 * party roster. Returns -2 for an empty ENTD slot, otherwise
 * main_party_save_unit's result (0, or -1 when the roster is full). guest_id is
 * passed on as that function's allow_guest flag. */
s32 main_unit_init(entd_encounter_t* entd, s32 unit_id, s32 guest_id, s32 initialize_for_battle) {
    entd_unit_t* entd_unit = &entd->units[unit_id];

    g_main_entd_current_unit = entd_unit;
    if (entd_unit->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
        return -2;
    }
    {
        unit_stats_t unit;
        unit_stats_t* unit_data = &unit;

        unit_data->battle_id = 0xff;
        unit_data->entd_slot = 0x20;
        unit_data->existence = BATTLE_UNIT_EXISTENCE_ACTIVE;
        unit_data->formation_index = BATTLE_FORMATION_INDEX_NONE;
        main_unit_init_for_battle(unit_data, entd_unit, initialize_for_battle, 0);
        return main_party_save_unit(unit_data, guest_id);
    }
}

/*
 * Copies an in-battle unit record into its persistent party slot.
 * Returns 0 on success and -1 when no free party slot exists.
 */
s32 main_party_save_unit(unit_stats_t* unit, s32 allow_guest) {
    party_unit_t* party;
    s32 slot;
    s32 index;
    u32 unit_slot;
    u8 kind;
    u8 palette;
    u16 birthday;
    s32 sprite_set;

    kind = unit->unit_flags & UNIT_FLAG_SAVE_FORMATION;
    if (unit->character_identity >= CHARACTER_IDENTITY_SELECTOR_FIRST || allow_guest == 0) {
        kind = 0;
    }
    index = unit->formation_index;
    unit_slot = index & 0xff;

    if (kind == 0 && index >= PARTY_GUEST_SLOT_FIRST && index < PARTY_ROSTER_SLOT_COUNT) {
        /* A non-guest save of a unit that currently sits in a guest slot. */
        main_party_remove_unit(index);
        index = main_party_find_free_slot(0, &palette);
        if (index == -1) {
            return -1;
        }
        slot = index;
    } else if (unit_slot >= BATTLE_UNIT_SLOT_COUNT) {
        /* GCC cross-jumps this search into the identical one above. */
        index = main_party_find_free_slot(kind, &palette);
        if (index == -1) {
            return -1;
        }
        slot = index;
    } else {
        main_party_find_free_slot(0, &palette);
        slot = unit_slot;
    }

    party = main_party_get_unit(slot);
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
    main_mem_copy_byte_data_source_first(&unit->secondary_skillset, &party->secondary_skillset, 0x10);
    party->bravery = unit->original_brave;
    party->faith = unit->original_faith;
    main_mem_copy_byte_data_source_first(unit->raw_stats, party->raw_stats, sizeof(party->raw_stats));
    /* The saved span includes the career record and following 16-byte name. */
    main_mem_copy_byte_data_source_first(&unit->career, &party->career, sizeof(party->career) + sizeof(party->name));
    /* name_id is stored as two byte writes (lbu / lhu+srl), not one sh. */
    party->name_id.fields.index = unit->quote_name_id.value;
    party->name_id.fields.name_class = unit->quote_name_id.value >> 8;
    party->proposition_status = 0;
    party->egg_color = 0;
    return 0;
}

/* Return the first empty roster slot, or -1 when the searched range is full.
 *
 * A nonzero save_formation searches the four guest slots (16..19), zero the
 * sixteen ordinary slots. *palette is always set to 0. */
s32 main_party_find_free_slot(s32 save_formation, u8* palette) {
    party_unit_t* party_units[PARTY_ROSTER_SLOT_COUNT];
    s32 party_index;
    s32 start_index;
    s32 limit;

    *palette = 0;

    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_units[party_index] = main_party_get_unit(party_index);
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
s32 main_party_create_monster_egg(s32 monster_job, s32 egg_modifier, s32 egg_color) {
    s32 party_index;
    party_unit_t* party;
    u16 birthday;
    s32 zodiac;

    party_index = main_party_generate_unit(MAIN_UNIT_TYPE_MONSTER);
    if (party_index == -1) {
        return party_index;
    }
    party = main_party_get_unit(party_index);
    party->job_id = monster_job;
    party->egg_color = egg_color;
    party->gender_flags |= UNIT_FLAG_EGG;
    birthday = egg_modifier;
    if (birthday >= 0x16e) {
        birthday = 1;
    }
    zodiac = main_unit_calculate_zodiac_symbol(birthday);
    party->birthday_day = birthday;
    party->zodiac = ((birthday & 0x100) >> 8) + (zodiac << 4);
    return party_index;
}

/* Generate a new unit of unit_type (MAIN_UNIT_TYPE_*) in the first empty ordinary
 * roster slot (0..15), with palette 0. Returns the slot, or -1 when all sixteen
 * are taken. */
s32 main_party_generate_unit(main_unit_generation_type_e unit_type) {
    party_unit_t* party_units[PARTY_ROSTER_SLOT_COUNT];
    s32 party_index;
    s32 palette = 0;

    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_units[party_index] = main_party_get_unit(party_index);
    }

    for (party_index = 0; party_index < PARTY_MEMBER_SLOT_COUNT; party_index++) {
        if (party_units[party_index]->party_id == PARTY_ID_NONE) {
            party_units[party_index]->party_id = party_index;
            party_units[party_index]->palette = palette;
            main_unit_generate_out_of_battle(party_units[party_index], unit_type);
            return party_index;
        }
    }

    return -1;
}

static s32 main_party_find_slot_by_sprite_set(s32 sprite_set) {
    s32 party_index = 0;
    s32 empty_party_id = PARTY_ID_NONE;
    party_unit_t* party = g_main_party_units;

    for (; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++, party++) {
        if (party->party_id != empty_party_id && party->sprite_set == sprite_set) {
            return party_index;
        }
    }

    return -1;
}

void main_party_remove_unit(u32 index) {
    g_main_party_units[index].party_id = PARTY_ID_NONE;
}

/* Fills a fresh party record for a generic male (0), generic female (1),
 * monster (3) or Ramza (anything else, forced to 2).
 *
 * The birthday is a random day 1..365. Brave and Faith are each rolled in
 * 40..70; Ramza gets 70 and 70. The unit starts at level 1 with 0 EXP, and a
 * human gets 100..199 JP and job level 1 in every job but the one its sex
 * cannot take (Bard, Dancer). The name is drawn until no party member of the
 * same kind has it. */
static void main_unit_generate_out_of_battle(party_unit_t* party, main_unit_generation_type_e unit_type) {
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

    main_mem_zero(&party->secondary_skillset, 7);
    main_unit_generate_party_base_raw_stats(party, unit_type);
    party->level = 1;
    party->experience = 0;
    main_mem_zero(&party->career, sizeof(party->career));

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
    main_mem_copy_bytes_source_first(world_text_find_entry(name_flags), party->name, sizeof(party->name));
    party->proposition_status = 0;
    party->egg_color = 0;
    if (unit_type == MAIN_UNIT_TYPE_RAMZA) {
        party->career.learned_abilities[0][0] = 4;
    }
}

/* The target leaves a1 intact while advancing a0 to the raw-stat field. */
static void main_unit_generate_party_base_raw_stats(party_unit_t* party, main_unit_generation_type_e unit_type) {
    main_unit_generate_base_raw_stats(party->raw_stats, unit_type);
}

/* Roll the unit's raw stats by its sex flags; a unit with neither flag is rolled
 * as a monster. */
static void main_unit_generate_raw_stats(unit_stats_t* unit) {
    main_unit_generation_type_e unit_type;

    if ((unit->unit_flags & UNIT_FLAG_MALE) != 0) {
        unit_type = MAIN_UNIT_TYPE_MALE;
    } else if ((unit->unit_flags & UNIT_FLAG_FEMALE) != 0) {
        unit_type = MAIN_UNIT_TYPE_FEMALE;
    } else {
        unit_type = MAIN_UNIT_TYPE_MONSTER;
    }
    main_unit_generate_base_raw_stats(unit->raw_stats, unit_type);
}

/* Roll the five little-endian 24-bit raw stats for the selected unit type. */
static void main_unit_generate_base_raw_stats(
    unit_raw_stat_t raw_stats[UNIT_RAW_STAT_COUNT], main_unit_generation_type_e unit_type) {
    /* Without the pin, type_index, index and base rotate among $s2/$s3/$s4. */
    register s32 type_index __asm__("$20") = unit_type;
    s32 index = 0;
    u8(*variance)[UNIT_RAW_STAT_COUNT] = g_main_unit_generation_raw_stat_variance;
    main_unit_generation_base_data_t* table = g_main_unit_generation_base_data;
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

static void main_unit_copy_job_growths_and_multipliers(unit_stats_t* unit) {
    main_mem_copy_byte_data_source_first(
        g_main_job_data_pointer[unit->job_id].stat_growths, unit->stat_growths, sizeof(unit->stat_growths));
}

/* Return the ability id in one slot of a skillset, or 0 for a slot or skillset
 * out of range.
 *
 * A human skillset (below SKILLSET_ID_MONSTER_FIRST) has 22 slots, a monster
 * skillset 4. Each record stores the low byte of every id; bit 8 of slot n comes
 * from the high_id_bits bytes, most significant bit first. */
s32 main_ability_get_id_from_skillset(s32 skillset_id, s32 ability_index) {
    if (skillset_id < SKILLSET_ID_MONSTER_FIRST) {
        main_ability_human_skillset_t* table_base;
        main_ability_human_skillset_t* skillset;
        s32 flag_byte_index;
        s32 high_id_bit;

        if (ability_index >= HUMAN_SKILLSET_ABILITY_COUNT) {
            return 0;
        }
        table_base = g_main_ability_human_skillset_abilities;
        skillset = table_base + skillset_id;
        flag_byte_index = ability_index / 8;
        high_id_bit = skillset->high_id_bits[flag_byte_index];
        high_id_bit <<= ability_index - flag_byte_index * 8 + 1;
        high_id_bit &= 0x100;
        return skillset->ability_ids[ability_index] | high_id_bit;
    }

    if (skillset_id < SKILLSET_ID_END) {
        main_ability_monster_skillset_t* table_base;
        main_ability_monster_skillset_t* skillset;
        s32 flag_byte_index;
        s32 high_id_bit;

        if (ability_index >= MONSTER_SKILLSET_ABILITY_COUNT) {
            return 0;
        }
        table_base = g_main_ability_monster_skillset_abilities - SKILLSET_ID_MONSTER_FIRST;
        skillset = table_base + skillset_id;
        flag_byte_index = ability_index / 8;
        high_id_bit = (&skillset->high_id_bits)[flag_byte_index];
        high_id_bit <<= ability_index - flag_byte_index * 8 + 1;
        high_id_bit &= 0x100;
        return skillset->ability_ids[ability_index] | high_id_bit;
    }

    return 0;
}

/* 0x80066204: scratch list of a skillset's usable ability ids. */
/* Copies the skillset's ability ids into g_main_ability_temp_list, keeping only the
 * classes selected by skillset_ability_filter_e. */
u16* main_ability_store_skillset_abilities(s32 skillset_id, s32 filters) {
    s32 output_count;
    s32 ability_index;
    u16 ability_id;
    s32 keep;

    if (skillset_id >= 0x100) {
        skillset_id = 0;
    }
    output_count = 0;
    for (ability_index = 0; ability_index < SKILLSET_ABILITY_LIST_COUNT; ability_index++) {
        ability_id = main_ability_get_id_from_skillset(skillset_id, ability_index);
        if (ability_id < ABILITY_ID_REACTION_FIRST) {
            keep = filters & SKILLSET_ABILITY_FILTER_ACTION;
        } else if (ability_id < ABILITY_ID_SUPPORT_FIRST) {
            keep = filters & SKILLSET_ABILITY_FILTER_REACTION;
        } else if (ability_id <= ABILITY_ID_MOVEMENT_FIRST) {
            /* One past ABILITY_ID_MOVEMENT_FIRST in the original. */
            keep = filters & SKILLSET_ABILITY_FILTER_SUPPORT;
        } else {
            keep = filters & SKILLSET_ABILITY_FILTER_MOVEMENT;
        }
        if (keep == 0) {
            ability_id = 0;
        }
        g_main_ability_temp_list[output_count++] = ability_id;
    }

    while (output_count < SKILLSET_ABILITY_LIST_COUNT) {
        g_main_ability_temp_list[output_count] = 0;
        output_count += 1;
    }
    return g_main_ability_temp_list;
}

/* Classify an ability id and return pointers to its two table records.
 *
 * *out_ability_data always receives the common 8-byte record. *out_secondary_data
 * receives the record of the id's range: the range/effect record of an ordinary
 * ability, the item id of an Item ability, the item type of a Throw ability, the
 * Jump or Charge record, or the one-byte record shared by Math, reaction, support
 * and movement abilities. The result is the matching ability_type_e. */
s32 main_ability_calculate_pointers_and_type(
    s32 ability_id, ability_data_t** out_ability_data, main_ability_secondary_pointer_t* out_secondary_data) {
    ability_id &= ABILITY_ID_MASK;
    /* 0x8005ebf0: Ability Data 1, 8 bytes each. */
    *out_ability_data = &g_main_ability_data[ability_id];
    if (ability_id < ABILITY_ID_ITEM_FIRST) {
        out_secondary_data->range = &g_main_ability_range_data[ability_id];
        return ABILITY_TYPE_DEFAULT;
    }
    if (ability_id < ABILITY_ID_THROW_FIRST) {
        out_secondary_data->item_id = &g_main_item_ability_item_ids[ability_id - ABILITY_ID_ITEM_FIRST];
        return ABILITY_TYPE_ITEM;
    }
    if (ability_id < ABILITY_ID_JUMP_FIRST) {
        out_secondary_data->throw_item_type = &g_main_throw_ability_item_types[ability_id - ABILITY_ID_THROW_FIRST];
        return ABILITY_TYPE_THROW;
    }
    if (ability_id < ABILITY_ID_CHARGE_FIRST) {
        out_secondary_data->jump = &g_main_jump_ability_data[ability_id - ABILITY_ID_JUMP_FIRST];
        return ABILITY_TYPE_JUMP;
    }
    if (ability_id < ABILITY_ID_MATH_FIRST) {
        out_secondary_data->charge = &g_main_charge_ability_data[ability_id - ABILITY_ID_CHARGE_FIRST];
        return ABILITY_TYPE_CHARGE;
    }
    if (ability_id < ABILITY_ID_REACTION_FIRST) {
        out_secondary_data->math_rsm = &g_main_math_rsm_ability_data[ability_id - ABILITY_ID_MATH_FIRST];
        return ABILITY_TYPE_MATH;
    }
    if (ability_id < ABILITY_ID_SUPPORT_FIRST) {
        out_secondary_data->math_rsm = &g_main_math_rsm_ability_data[ability_id - ABILITY_ID_MATH_FIRST];
        return ABILITY_TYPE_REACTION;
    }
    if (ability_id < ABILITY_ID_MOVEMENT_FIRST) {
        out_secondary_data->math_rsm = &g_main_math_rsm_ability_data[ability_id - ABILITY_ID_MATH_FIRST];
        return ABILITY_TYPE_SUPPORT;
    }
    out_secondary_data->math_rsm = &g_main_math_rsm_ability_data[ability_id - ABILITY_ID_MATH_FIRST];
    return ABILITY_TYPE_MOVEMENT;
}

item_data_t* main_item_get_data_pointer(s32 item_id) {
    return &g_main_item_primary_data[item_id & ITEM_ID_MASK];
}

job_data_t* main_job_get_data_pointer(s32 job_id) {
    if (job_id < JOB_ID_COUNT) {
        return &g_main_job_data[job_id];
    }
    return 0;
}

/* Rebuild one runtime unit from its persistent party slot.
 *
 * use_world_text selects the name-text provider used by the shared final
 * setup. Returns -1 when the requested party slot is unavailable. */
s32 main_unit_init_job_data(unit_stats_t* unit, s32 slot, s32 use_world_text) {
    party_unit_t* party_unit;

    g_main_unit_name_uses_world_text = use_world_text;
    party_unit = main_party_get_unit(slot);
    if (party_unit == 0) {
        return -1;
    }
    if (party_unit->party_id == PARTY_ID_NONE) {
        return -1;
    }
    main_unit_init_job_levels(party_unit->career.job_points.total.values, party_unit->career.job_levels);
    main_job_store_unlock_bitset(party_unit->career.unlocked_jobs,
        main_job_calculate_unlocked(party_unit->career.job_levels, party_unit->gender_flags));
    main_unit_reset_battle_state(unit);
    unit->formation_index = slot;
    main_unit_init_battle_data(unit, party_unit);
    main_unit_copy_job_data(unit);
    main_unit_enable_rsm_flags(unit);
    main_unit_calculate_actual_stats(unit, 0);
    main_unit_set_equippable_items(unit);
    main_unit_apply_equipment_move_jump_and_name(unit);
    return 0;
}

/* Build one runtime unit from an ENTD definition.
 *
 * use_world_text selects the WORLD name provider during the shared final
 * setup. A temporary formation index enables level growth for generated units.
 * Mode 0x82 is the transformed-monster recalculation path and preserves the
 * existing runtime/ENTD-derived state. */
s32 main_unit_init_for_battle(unit_stats_t* unit, entd_unit_t* entd, u32 use_world_text, s32 kind) {
    g_main_unit_name_uses_world_text = use_world_text;
    /* Kind 0x82 skips the reset and the ENTD calculation. */
    if (kind != 0x82) {
        main_unit_reset_battle_state(unit);
        if (main_unit_calculate_entd_data(unit, entd) != 0) {
            return -1;
        }
    }
    if (kind == 0) {
        if ((entd->unit_flags & UNIT_FLAG_LOAD_FORMATION) != 0) {
            return 0;
        }
        if (entd->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
            if (entd->birth_month == 0) {
                return 0;
            }
        }
    }
    main_unit_calculate_jobs_and_skillsets_from_entd(unit, entd);
    main_unit_calculate_abilities(unit, entd);
    main_unit_enable_rsm_flags(unit);
    main_unit_generate_raw_stats(unit);
    main_unit_copy_job_growths_and_multipliers(unit);
    if (unit->formation_index == BATTLE_FORMATION_INDEX_NONE) {
        unit->formation_index = BATTLE_FORMATION_INDEX_GENERATED_UNIT;
    }
    main_unit_calculate_actual_stats(unit, 0);
    if (unit->formation_index == BATTLE_FORMATION_INDEX_GENERATED_UNIT) {
        unit->formation_index = BATTLE_FORMATION_INDEX_NONE;
    }
    main_unit_set_equippable_items(unit);
    if (kind != 0x82) {
        if (unit->formation_index >= PARTY_ROSTER_SLOT_COUNT) {
            main_unit_calculate_entd_equipment(unit, entd);
        }
    }
    main_unit_apply_equipment_move_jump_and_name(unit);
    main_unit_store_ramza_name_birthday_zodiac(unit);
    return 0;
}

static void main_unit_apply_equipment_move_jump_and_name(unit_stats_t* unit) {
    main_unit_set_equipment_stats(unit);
    main_unit_set_equipment_attributes(unit, 1);
    main_unit_calculate_move_jump(unit, 0);
    main_unit_store_character_names(unit);
}

/* For a unit with one of Ramza's three identities, copy the name, birthday and
 * zodiac sign from the first roster record that has one of them. */
static void main_unit_store_ramza_name_birthday_zodiac(unit_stats_t* unit) {
    s32 slot;
    party_unit_t* party;

    if (unit->character_identity > CHARACTER_IDENTITY_RAMZA_CHAPTER_4) {
        return;
    }
    for (slot = 0; slot < PARTY_ROSTER_SLOT_COUNT; slot++) {
        party = main_party_get_unit(slot);
        if (party->party_id == PARTY_ID_NONE) {
            continue;
        }
        if (party->sprite_set > CHARACTER_IDENTITY_RAMZA_CHAPTER_4) {
            continue;
        }
        unit->birthday.birthday = party->birthday_day + (party->zodiac << 8);
        unit->birthday.zodiac = party->zodiac >> PARTY_ZODIAC_SHIFT;
        main_mem_copy_byte_data_source_first(party->name, unit->name, sizeof(unit->name));
        return;
    }
}

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
static s32 main_unit_calculate_entd_data(unit_stats_t* unit, entd_unit_t* entd) {
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

/* Load the party unit selected by an ENTD sprite set.
 *
 * Preserve the party unit's kind flags while taking the ENTD persistence
 * controls and team assignment. Returns -1 when no matching party slot exists. */
static s32 main_unit_init_job_data_from_entd(unit_stats_t* unit, entd_unit_t* entd_unit) {
    s32 slot;

    slot = main_party_find_slot_by_sprite_set(entd_unit->sprite_set);
    if (slot != -1) {
        main_unit_init_job_data(unit, slot, g_main_unit_name_uses_world_text);
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
 * Fills an in-battle unit record from its persistent party record.
 */
static void main_unit_init_battle_data(unit_stats_t* unit, const party_unit_t* party) {
    s32 team;
    s32 i;
    s32 value;

    team = 8;
    if (party->sprite_set < CHARACTER_IDENTITY_RAMZA_END) {
        team = 0xb;
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
    unit->abilities.reaction = party->reaction_ability[0] + (party->reaction_ability[1] << 8);
    unit->abilities.support = party->support_ability[0] + (party->support_ability[1] << 8);
    unit->abilities.movement = party->movement_ability[0] + (party->movement_ability[1] << 8);
    main_mem_copy_byte_data_source_first(party->equipment, unit->equipment, UNIT_EQUIPMENT_SLOT_COUNT);
    unit->experience = party->experience;
    unit->level = party->level;
    unit->original_brave = unit->brave = party->bravery;
    unit->original_faith = unit->faith = party->faith;
    main_mem_copy_byte_data_source_first(party->raw_stats, unit->raw_stats, sizeof(unit->raw_stats));
    for (i = 0; i < UNIT_UNLOCKED_JOB_BYTE_COUNT; i++) {
        unit->career.unlocked_jobs[i] = party->career.unlocked_jobs[i];
    }
    main_mem_copy_byte_data_source_first(
        party->career.learned_abilities, unit->career.learned_abilities, UNIT_LEARNED_ABILITY_BYTE_COUNT);
    main_mem_copy_byte_data_source_first(party->career.job_levels, unit->career.job_levels, UNIT_JOB_LEVEL_BYTE_COUNT);
    /* Copy the adjacent current/total JP tables as their 80 serialized bytes. */
    main_mem_copy_byte_data_source_first(
        &party->career.job_points, &unit->career.job_points, sizeof(unit->career.job_points));
    main_mem_copy_byte_data_source_first(party->name, unit->name, sizeof(unit->name));
    unit->quote_name_id.value = party->name_id.fields.index + (party->name_id.fields.name_class << 8);
    main_mem_zero(&unit->ai_target_x, 7);
}

/* Initialize packed job levels, unlocked jobs, and ENTD skillset overrides. */
static void main_unit_calculate_jobs_and_skillsets_from_entd(unit_stats_t* unit, const entd_unit_t* entd) {
    s32 i;
    u32 unlocked;
    u32 flags;
    u8 selected;
    u16 skillset;
    u8 levels;

    main_mem_zero(unit->career.job_levels, UNIT_JOB_LEVEL_BYTE_COUNT);
    unlocked = 0;
    if (!entd) {
        main_unit_copy_job_data(unit);
        unit->secondary_skillset = 0;
        unit->unit_flags = UNIT_FLAG_MONSTER;
        main_job_store_unlock_bitset(unit->career.unlocked_jobs, 0);
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
                        levels = g_main_job_unlock_requirements[selected - 1][i];
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
                unlocked = main_job_calculate_unlocked(unit->career.job_levels, flags);
            }
        }
        main_job_store_unlock_bitset(unit->career.unlocked_jobs, unlocked);
        unit->job_id = entd->job_id;
        main_unit_copy_job_data(unit);
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
                levels = main_job_get_random_unlocked(unit);
                if (levels)
                    skillset = g_main_job_data_pointer[levels].skillset;
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

enum {
    RANDOM_JOB_COUNT = JOB_ID_MIME - JOB_ID_GENERIC_FIRST,
};

/* Return a random unlocked generic job ID below Mime, or 0 when none are unlocked.
 *
 * The ENTD caller explicitly normalizes the zero-extended job byte. */
static s32 main_job_get_random_unlocked(const unit_stats_t* unit) {
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
static void main_unit_copy_job_data(unit_stats_t* unit) {
    job_data_t* job;
    s32 i;
    s32 flag_a;
    s32 flag_b;

    if (unit->character_identity < CHARACTER_IDENTITY_GENERIC_FIRST) {
        unit->base_job_skillset = g_main_job_data_pointer[unit->character_identity].skillset;
    } else {
        unit->base_job_skillset = 0;
    }

    job = g_main_job_data_pointer + unit->job_id;
    unit->primary_skillset = job->skillset;
    main_mem_copy_byte_data_source_first(job->innate_abilities, unit->innate_abilities, sizeof(unit->innate_abilities));
    main_mem_copy_byte_data_source_first(
        job->equipment_categories, unit->equipment_categories, sizeof(unit->equipment_categories));
    main_mem_copy_byte_data_source_first(job->stat_growths, unit->stat_growths, sizeof(unit->stat_growths));
    unit->move = job->move;
    unit->jump = job->jump & JOB_JUMP_VALUE_MASK;
    if (job->jump & JOB_JUMP_STEPPING_STONE) {
        unit->position.stepping_stone = 1;
    } else {
        unit->position.stepping_stone = 0;
    }
    main_mem_copy_byte_data_source_first(&job->status_sets, &unit->status_sets, sizeof(job->status_sets));

    flag_a = unit->team_flags & BATTLE_TEAM_FLAG_IMMORTAL;
    flag_b = unit->unit_flags & (UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION);
    if (flag_a != 0 && flag_b != 0) {
        u8* statuses = unit->status_sets.immunity;
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            if (flag_a != 0) {
                statuses[i] |= g_main_status_check_sets[MAIN_STATUS_CHECK_SET_IMMORTAL_IMMUNITY][i];
            }
            if (flag_b != 0) {
                statuses[i] |= g_main_status_check_sets[MAIN_STATUS_CHECK_SET_FORMATION_IMMUNITY][i];
            }
        }
    }

    main_mem_copy_byte_data_source_first(
        job->elemental_affinity, unit->elemental_affinity, sizeof(job->elemental_affinity));
    unit->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN] = 0;
    unit->spritesheet_id = job->spritesheet_id;
    unit->job_portrait_palette = job->job_portrait_palette;
    unit->monster_graphic = job->monster_graphic;
}

/* Rebuilds the unit's active reaction/support/movement bitfields
 * (0x8b..0x95) from its four innate abilities and equipped R/S/M
 * abilities. */
void main_unit_enable_rsm_flags(unit_stats_t* unit) {
    s32 i;

    main_mem_zero(&unit->ability_flags, sizeof(unit->ability_flags));
    for (i = 0; i < 4; i++) {
        main_unit_set_rsm_flag(unit, unit->innate_abilities[i]);
    }
    main_unit_set_rsm_flag(unit, unit->abilities.reaction);
    main_unit_set_rsm_flag(unit, unit->abilities.support);
    main_unit_set_rsm_flag(unit, unit->abilities.movement);
}

/* Mark an R/S/M ability active and select reaction abilities.
 *
 * IDs 0x1a6..0x1fd map contiguously into the unit's MSB-first active flags.
 * A reaction ID also becomes the unit's currently equipped reaction ability. */
static void main_unit_set_rsm_flag(unit_stats_t* unit, u32 ability_id) {
    u16 index;
    u16 original_ability_id;
    u8* rsm_flags;

    index = ability_id - ABILITY_ID_REACTION_FIRST;
    original_ability_id = ability_id;
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
static void main_unit_calculate_actual_stats(unit_stats_t* unit, s32 mode) {
    u32 value;
    u32 multiplier;
    s32 cap;
    s32 max;
    s32 i;
    s32 j;
    u8 level;
    s32 brave_flag;
    s32 divisor;
    s32 is_low;
    job_stat_growth_t* mult;
    unit_raw_stat_t* raw;

    cap = 100;
    max = UNIT_HP_MP_MAX;
    brave_flag = mode & 1;
    if (unit->unit_flags & UNIT_FLAG_EGG) {
        cap = 10;
        max = 0xffff;
    }
    mult = unit->stat_growths;
    level = unit->level;
    raw = unit->raw_stats;

    for (i = 0; i < UNIT_RAW_STAT_COUNT; i++) {
        value = raw[i].low + (raw[i].middle << 8) + (raw[i].high << 16);
        if (mode == 0 && unit->formation_index == BATTLE_FORMATION_INDEX_GENERATED_UNIT) {
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
            divisor = cap;
        }
        value = (value * multiplier) / divisor >> 14;
        if (value == 0) {
            value = 1;
        }
        if (is_low) {
            if (value > max) {
                value = max;
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
            if (brave_flag == 0) {
                unit->base_attributes[UNIT_ATTRIBUTE_SPEED] = value;
            }
            break;
        case UNIT_RAW_STAT_PHYSICAL_ATTACK:
            if (brave_flag == 0) {
                unit->base_attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK] = value;
            }
            break;
        case UNIT_RAW_STAT_MAGIC_ATTACK:
            if (brave_flag == 0) {
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
 * Random JP for a job level: g_main_job_jp_requirements_by_level[level] (the
 * JP needed to reach it; 0x80066182, entry 0 is 0) plus rand(0..99), or
 * 100 + rand(0..99) for level 0. */
static void main_unit_calculate_abilities(unit_stats_t* unit, entd_unit_t* entd) {
    u8 job_id = unit->job_id;
    u8 gender = unit->unit_flags;
    u8 byte;
    s32 i;
    s32 level;
    u16 value;

    if (entd == 0) {
        main_mem_zero(&unit->abilities, sizeof(unit->abilities));
        return;
    }

    byte = entd->primary_skillset;
    if (byte != 0xff && byte != 0) {
        unit->primary_skillset = byte;
    } else {
        unit->primary_skillset = g_main_job_data_pointer[job_id].skillset;
    }

    if (gender & (UNIT_FLAG_MALE | UNIT_FLAG_FEMALE)) {
        main_mem_zero(unit->career.learned_abilities, UNIT_LEARNED_ABILITY_BYTE_COUNT);
        for (i = 0; i < UNIT_JOB_LEVEL_BYTE_COUNT; i++) {
            byte = unit->career.job_levels[i];
            level = byte >> 4;
            if (i == UNIT_JOB_LEVEL_BYTE_COUNT - 1 && (gender & UNIT_FLAG_MALE)) {
                value = 0; /* Dancer is female only */
            } else if (level != 0) {
                value = g_main_job_jp_requirements_by_level[level] + rand() * 100 / 0x8000;
            } else {
                value = rand() * 100 / 0x8000 + 100;
            }
            unit->career.job_points.total.values[i * 2] = value;
            unit->career.job_points.current.values[i * 2] = value;
            main_unit_learn_job_abilities(unit, i * 2 + JOB_ID_SQUIRE, entd);

            level = byte & 0xf;
            if (i == UNIT_JOB_LEVEL_BYTE_COUNT - 2 && (gender & UNIT_FLAG_FEMALE)) {
                value = 0; /* Bard is male only */
            } else if (level != 0) {
                value = g_main_job_jp_requirements_by_level[level] + rand() * 100 / 0x8000;
            } else {
                value = rand() * 100 / 0x8000 + 100;
            }
            unit->career.job_points.total.values[i * 2 + 1] = value;
            unit->career.job_points.current.values[i * 2 + 1] = value;
            if (i == UNIT_JOB_LEVEL_BYTE_COUNT - 1) {
                break; /* Mime's JP is initialized, but its learning call is skipped. */
            }
            main_unit_learn_job_abilities(unit, i * 2 + JOB_ID_SQUIRE + 1, entd);
        }

        if (entd->primary_skillset != 0 && entd->primary_skillset != SKILLSET_ID_NONE) {
            unit->career.learned_abilities[0][0] = 0xff;
            unit->career.learned_abilities[0][1] = 0xff;
            unit->career.learned_abilities[0][2] = 0xff;
        }
    }

    for (i = 0; i < 4; i++) {
        s32 k = i * 2;

        unit->innate_abilities[i] = g_main_job_data_pointer[job_id].innate_abilities[k]
            + (g_main_job_data_pointer[job_id].innate_abilities[k + 1] << 8);
        unit->equipment_categories[i] = g_main_job_data_pointer[job_id].equipment_categories[i];
    }

    unit->abilities.reaction
        = main_unit_calculate_rsm(unit, entd->abilities.reaction, RSM_ABILITY_FILTER_REACTION, entd);
    unit->abilities.support = main_unit_calculate_rsm(unit, entd->abilities.support, RSM_ABILITY_FILTER_SUPPORT, entd);
    unit->abilities.movement
        = main_unit_calculate_rsm(unit, entd->abilities.movement, RSM_ABILITY_FILTER_MOVEMENT, entd);
}

/* Map ENTD's five equipment bytes into the seven simulation slots. */
static void main_unit_store_monster_equipment(unit_stats_t* unit, const entd_unit_t* entd) {
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
 * A monster takes main_unit_store_monster_equipment instead. The random right
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
static void main_unit_calculate_entd_equipment(unit_stats_t* unit, entd_unit_t* entd) {
    u8 item;
    u8 flags;
    s32 i;

    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        main_unit_store_monster_equipment(unit, entd);
        return;
    }

    /* Right hand */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_RIGHT_HAND];
    flags = unit->ability_flags.support[2];
    if (item == ENTD_VALUE_RANDOM) {
        if (flags & BATTLE_SUPPORT_SET_3_TWO_HANDS) {
            item = main_unit_calculate_random_equipment(
                unit, ITEM_TYPE_FLAG_WEAPON, WEAPON_FLAG_TWO_HANDS_COMPATIBLE, ITEM_TYPE_FILTER_ANY);
        }
        if (item == ENTD_VALUE_RANDOM && (flags & BATTLE_SUPPORT_SET_3_TWO_SWORDS)) {
            item = main_unit_calculate_random_equipment(
                unit, ITEM_TYPE_FLAG_WEAPON, WEAPON_FLAG_TWO_SWORDS_COMPATIBLE, ITEM_TYPE_FILTER_ANY);
        }
    }
    flags = unit->ability_flags.support[0];
    if (item == ENTD_VALUE_RANDOM) {
        if (flags & BATTLE_SUPPORT_SET_1_EQUIP_SWORD) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_SWORD);
        }
        if (item == ENTD_VALUE_RANDOM) {
            if (flags & BATTLE_SUPPORT_SET_1_EQUIP_KATANA) {
                item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_KATANA);
            }
            if (item == ENTD_VALUE_RANDOM) {
                if (flags & BATTLE_SUPPORT_SET_1_EQUIP_CROSSBOW) {
                    item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_CROSSBOW);
                }
                if (item == ENTD_VALUE_RANDOM) {
                    if (flags & BATTLE_SUPPORT_SET_1_EQUIP_SPEAR) {
                        item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_SPEAR);
                    }
                    if (item == ENTD_VALUE_RANDOM) {
                        if (flags & BATTLE_SUPPORT_SET_1_EQUIP_AXE) {
                            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_AXE);
                        }
                        if (item == ENTD_VALUE_RANDOM) {
                            if (flags & BATTLE_SUPPORT_SET_1_EQUIP_GUN) {
                                item = main_unit_calculate_random_equipment(
                                    unit, ITEM_TYPE_FLAG_WEAPON, 0, ITEM_TYPE_GUN);
                            }
                            if (item == ENTD_VALUE_RANDOM) {
                                item = main_unit_calculate_random_equipment(
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
    if (g_main_item_primary_data[item].type_flags & ITEM_TYPE_FLAG_SHIELD) {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item;
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = ITEM_ID_NONE;
    } else {
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item;
        unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = ITEM_ID_NONE;
    }

    /* Left hand */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_LEFT_HAND];
    if (unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] != ITEM_ID_NONE
        && ((g_main_item_weapon_data[g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]]
                                         .secondary_data_id]
                    .flags
                & WEAPON_FLAG_FORCED_TWO_HANDS)
            || (unit->ability_flags.support[2] & BATTLE_SUPPORT_SET_3_TWO_HANDS))) {
        item = ITEM_ID_NONE;
    }
    flags = unit->ability_flags.support[2];
    if (item == ENTD_VALUE_RANDOM && (flags & BATTLE_SUPPORT_SET_3_TWO_SWORDS)) {
        item = main_unit_calculate_random_equipment(
            unit, ITEM_TYPE_FLAG_WEAPON, WEAPON_FLAG_TWO_SWORDS_COMPATIBLE, ITEM_TYPE_FILTER_ANY);
    }
    flags = unit->equipment_categories[EQUIPPABLE_ITEM_TYPE_BYTE_INDEX(ITEM_TYPE_SHIELD)];
    if (item == ENTD_VALUE_RANDOM) {
        if (flags & EQUIPPABLE_ITEM_TYPE_BYTE_MASK(ITEM_TYPE_SHIELD)) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_SHIELD, 0, ITEM_TYPE_FILTER_ANY);
        } else {
            item = ITEM_ID_NONE;
        }
    }
    if (g_main_item_primary_data[item].type_flags & ITEM_TYPE_FLAG_WEAPON) {
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
        if (flags & BATTLE_SUPPORT_SET_1_EQUIP_ARMOR) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_HEADGEAR, 0, ITEM_TYPE_HELMET);
        }
        if (item == ENTD_VALUE_RANDOM) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_HEADGEAR, 0, ITEM_TYPE_FILTER_ANY);
            if (item == ENTD_VALUE_RANDOM) {
                item = ITEM_ID_NONE;
            }
        }
    }
    unit->equipment[UNIT_EQUIPMENT_SLOT_HEAD] = item;

    /* Body */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_BODY];
    if (item == ENTD_VALUE_RANDOM) {
        /* The target reuses Equip Shield here while requesting random Armor;
         * preserve this native equipment-generation quirk. */
        if (flags & BATTLE_SUPPORT_SET_1_EQUIP_SHIELD) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_BODY_ARMOR, 0, ITEM_TYPE_ARMOR);
        }
        if (item == ENTD_VALUE_RANDOM) {
            item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_BODY_ARMOR, 0, ITEM_TYPE_FILTER_ANY);
            if (item == ENTD_VALUE_RANDOM) {
                item = ITEM_ID_NONE;
            }
        }
    }
    unit->equipment[UNIT_EQUIPMENT_SLOT_BODY] = item;

    /* Accessory */
    item = entd->equipment[ENTD_EQUIPMENT_SLOT_ACCESSORY];
    if (item == ENTD_VALUE_RANDOM) {
        item = main_unit_calculate_random_equipment(unit, ITEM_TYPE_FLAG_ACCESSORY, 0, ITEM_TYPE_FILTER_ANY);
        if (item == ENTD_VALUE_RANDOM) {
            item = ITEM_ID_NONE;
        }
    }
    unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY] = item;

    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        if (unit->equipment[i] == ITEM_ID_NOTHING) {
            unit->equipment[i] = ITEM_ID_NONE;
        }
    }
}

/* Add to the unit's equippable item types those its Equip support abilities grant
 * (Equip Armor covers helmets and armor), and the ribbon, bag and perfume types
 * for a female unit. */
void main_unit_set_equippable_items(unit_stats_t* unit) {
    s32 support = unit->ability_flags.support[0];

    if (support & BATTLE_SUPPORT_SET_1_EQUIP_ARMOR) {
        unit->equipment_categories[2] |= BATTLE_EQUIPMENT_SET_3_HELM | BATTLE_EQUIPMENT_SET_3_ARMOR;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_SHIELD) {
        unit->equipment_categories[2] |= BATTLE_EQUIPMENT_SET_3_SHIELD;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_SWORD) {
        unit->equipment_categories[0] |= BATTLE_EQUIPMENT_SET_1_SWORD;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_KATANA) {
        unit->equipment_categories[0] |= BATTLE_EQUIPMENT_SET_1_KATANA;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_CROSSBOW) {
        unit->equipment_categories[1] |= BATTLE_EQUIPMENT_SET_2_CROSSBOW;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_SPEAR) {
        unit->equipment_categories[1] |= BATTLE_EQUIPMENT_SET_2_SPEAR;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_AXE) {
        unit->equipment_categories[0] |= BATTLE_EQUIPMENT_SET_1_AXE;
    }
    if (support & BATTLE_SUPPORT_SET_1_EQUIP_GUN) {
        unit->equipment_categories[1] |= BATTLE_EQUIPMENT_SET_2_GUN;
    }
    support = unit->unit_flags;
    if (support & UNIT_FLAG_FEMALE) {
        unit->equipment_categories[2] |= BATTLE_EQUIPMENT_SET_3_RIBBON | BATTLE_EQUIPMENT_SET_3_BAG;
        unit->equipment_categories[3] |= BATTLE_EQUIPMENT_SET_4_PERFUME;
    }
}

/* Fill unit->equipment_stats from the equipped items: the job's class evade, the
 * accessory's and each shield's physical and magic evade, and each weapon's power
 * and evade. A slot whose item is not of the expected kind leaves zeros, and a
 * monster gets only the class evade. */
void main_unit_set_equipment_stats(unit_stats_t* unit) {
    item_data_t* item;
    item_evade_t* shield;
    weapon_data_t* weapon;

    main_mem_zero(unit->equipment_stats, sizeof(unit->equipment_stats));
    unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_CLASS_PHYSICAL_EVADE]
        = g_main_job_data_pointer[unit->job_id].class_evade;
    if (unit->unit_flags & UNIT_FLAG_MONSTER) {
        return;
    }

    item = &g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]];
    if (item->type_flags & ITEM_TYPE_FLAG_ACCESSORY) {
        item_evade_t* accessory
            = &g_main_item_accessory_data[g_main_item_secondary_data_ids[unit->equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]
                * sizeof(item_data_t)]];
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_PHYSICAL_EVADE] = accessory->physical_evade;
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_ACCESSORY_MAGIC_EVADE] = accessory->magic_evade;
    }
    item = &g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD]];
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        shield = &g_main_item_shield_data[g_main_item_secondary_data_ids
                [unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] * sizeof(item_data_t)]];
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_PHYSICAL_EVADE] = shield->physical_evade;
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_SHIELD_MAGIC_EVADE] = shield->magic_evade;
    }
    item = &g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD]];
    if (item->type_flags & ITEM_TYPE_FLAG_SHIELD) {
        shield = &g_main_item_shield_data[g_main_item_secondary_data_ids
                [unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] * sizeof(item_data_t)]];
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_PHYSICAL_EVADE] = shield->physical_evade;
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_SHIELD_MAGIC_EVADE] = shield->magic_evade;
    }
    item = &g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]];
    if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
        weapon = &g_main_item_weapon_data[unit->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON]];
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_POWER] = weapon->power;
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_RIGHT_WEAPON_EVADE] = weapon->evade;
    }
    item = &g_main_item_primary_data[unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]];
    if (item->type_flags & ITEM_TYPE_FLAG_WEAPON) {
        weapon = &g_main_item_weapon_data[unit->equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON]];
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_POWER] = weapon->power;
        unit->equipment_stats[BATTLE_UNIT_EQUIPMENT_STAT_LEFT_WEAPON_EVADE] = weapon->evade;
    }
}

/* Applies equipment attribute bonuses to a unit's stats.
 *
 * Clears the equipment PA/MA/SP bonuses, recomputes the base stats, then
 * folds in each equipped item's attribute record and head/body HP/MP bonuses.
 * Actual PA/MA cap at 99 and Speed at 50. */
void main_unit_set_equipment_attributes(unit_stats_t* unit, s32 level_up_check) {
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

    main_mem_zero(unit->equipment_attributes, sizeof(unit->equipment_attributes));
    cap = UNIT_HP_MP_MAX;
    main_unit_calculate_actual_stats(unit, level_up_check + 1);
    unit->equipped_flags = 0;
    if ((unit->unit_flags & UNIT_FLAG_MONSTER) == 0) {
        bonus = unit->equipment_attributes;
        for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
            item_id = unit->equipment[i];
            if (item_id == ITEM_ID_MATERIA_BLADE) {
                unit->equipped_flags |= BATTLE_UNIT_EQUIPPED_FLAG_MATERIA_BLADE;
            }
            if (item_id < ITEM_ID_SHIELD_FIRST) {
                if (g_main_item_primary_data[item_id].type == ITEM_TYPE_SWORD
                    || g_main_item_primary_data[item_id].type == ITEM_TYPE_KNIGHT_SWORD) {
                    unit->equipped_flags |= BATTLE_UNIT_EQUIPPED_FLAG_SWORD;
                }
            }
            if (item_id == ITEM_ID_NONE) {
                continue;
            }
            item = &g_main_item_primary_data[item_id];
            attribute = &g_main_item_attributes[item->attributes];
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
            j = 0;
            target = unit->status_sets.innate;
            for (; j < 15; j++) {
                *target = attribute->status_sets.innate[j] | *target;
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
                value = unit->max_hp + g_main_item_helm_armor_secondary_data[second_id].hp_bonus;
                if (value > cap) {
                    value = cap;
                }
                unit->max_hp = value;
                value = unit->max_mp + g_main_item_helm_armor_secondary_data[second_id].mp_bonus;
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
 * capped at 0xFC and Jump at 7. With keep_position 0, HP and MP are refilled
 * first. */
void main_unit_calculate_move_jump(unit_stats_t* unit, s32 keep_position) {
    u16 move;
    u16 jump;
    u8 flags;

    if (keep_position == 0) {
        unit->hp = unit->max_hp;
        unit->mp = unit->max_mp;
    }
    move = unit->move;
    flags = unit->ability_flags.movement[0];
    jump = unit->jump;
    if (flags & BATTLE_MOVEMENT_SET_1_MOVE_PLUS_1) {
        move += 1;
    }
    if (flags & BATTLE_MOVEMENT_SET_1_MOVE_PLUS_2) {
        move += 2;
    }
    if (flags & BATTLE_MOVEMENT_SET_1_MOVE_PLUS_3) {
        move += 3;
    }
    if (flags & BATTLE_MOVEMENT_SET_1_JUMP_PLUS_1) {
        jump += 1;
    }
    if (flags & BATTLE_MOVEMENT_SET_1_JUMP_PLUS_2) {
        jump += 2;
    }
    if (flags & BATTLE_MOVEMENT_SET_1_JUMP_PLUS_3) {
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

enum {
    NAME_GENDER_MASK = UNIT_FLAG_MALE | UNIT_FLAG_FEMALE | UNIT_FLAG_MONSTER,
};

/* Copies the unit's name, job name and skillset names into its battle
 * record.  Units without a party slot get a random unused generic name for
 * their gender. */
static void main_unit_store_character_names(unit_stats_t* unit) {
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
    unit_stats_t* other;
    const unit_name_id_t* name_id_record;

    prepare_text = battle_text_find_entry;
    if (g_main_unit_name_uses_world_text != 0) {
        prepare_text = world_text_find_entry;
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
                    party = &g_main_party_units[i];
                    if (party->party_id != PARTY_ID_NONE && gender == (party->gender_flags & NAME_GENDER_MASK)) {
                        name_id_record = &party->name_id;
                        existing_name.value = 0;
                        existing_name.fields.name_class = name_id_record->fields.name_class;
                        existing_name.fields.index = name_id_record->fields.index;
                        if (existing_name.value == name_id.value) {
                            name_available = 0;
                            break;
                        }
                    }
                }
                for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                    other = &g_battle_unit_stats[i];
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
        text = main_party_get_unit(unit->formation_index)->name;
    }
    main_mem_copy_bytes_source_first(text, unit->name, sizeof(unit->name));
    text_id = unit->job_id | TEXT_ID_JOB_NAME_BASE;
    main_mem_copy_bytes_source_first(prepare_text(text_id), unit->job_name, sizeof(unit->job_name));
    text_id = unit->primary_skillset | TEXT_ID_SKILLSET_NAME_BASE;
    main_mem_copy_bytes_source_first(
        prepare_text(text_id), unit->primary_skillset_name, sizeof(unit->primary_skillset_name));
    text_id = unit->secondary_skillset | TEXT_ID_SKILLSET_NAME_BASE;
    main_mem_copy_bytes_source_first(
        prepare_text(text_id), unit->secondary_skillset_name, sizeof(unit->secondary_skillset_name));
    main_mem_zero(&unit->action.status_changes, sizeof(unit->action.status_changes));
}

/* Return the highest level in the roster, capped at 99, and store it in
 * g_main_party_highest_level. */
u8 main_party_calculate_highest_level(void) {
    u8 highest;
    s32 i;
    party_unit_t* unit;

    highest = 0;
    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = main_party_get_unit(i);
        if (unit->party_id != PARTY_ID_NONE && highest < unit->level) {
            highest = unit->level;
        }
    }
    if (highest > UNIT_LEVEL_MAX) {
        highest = UNIT_LEVEL_MAX;
    }
    g_main_party_highest_level = highest;
    return highest;
}

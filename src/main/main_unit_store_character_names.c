#include "fft/battle.h"
#include "fft/main.h"
#include "fft/world.h"
#include "psx/libc.h"

enum {
    NAME_GENDER_MASK = 0xe0,
};

/* Copies the unit's name, job name and skillset names into its battle
 * record.  Units without a party slot get a random unused generic name for
 * their gender. */
void main_unit_store_character_names(battle_stats_t* unit) {
    u8 unused[0x100];
    u8* (*prepare_text)(u32 text_id);
    u32 name_flags;
    u32 name_mod;
    u8 gender;
    u32 name_id;
    s32 unused_name;
    s32 i;
    u8* text;
    party_data_t* party;
    party_data_t* party_base;
    battle_stats_t* other;
    u8* name_bytes;

    prepare_text = battle_text_init_entry;
    if (g_main_unit_name_uses_world_text != 0) {
        prepare_text = world_text_find_entry;
    }
    if (unit->formation_index == PARTY_ID_NONE) {
        name_flags = TEXT_ID_UNIT_NAME_SPECIAL_BASE;
        name_id = (u8)unit->quote_name_id;
        if (name_id == 0xff) {
            gender = unit->unit_flags & NAME_GENDER_MASK;
            if (unit->unit_flags & UNIT_FLAG_MALE) {
                name_flags = TEXT_ID_UNIT_NAME_GENERIC_MALE_BASE;
                name_mod = 0x100;
            } else if (unit->unit_flags & UNIT_FLAG_FEMALE) {
                /* The empty loop wrapper only raises this variable's reference
                 * weight so the allocator keeps it in $s1; it emits no code. */
                do {
                    name_flags = TEXT_ID_UNIT_NAME_GENERIC_FEMALE_BASE;
                } while (0);
                name_mod = 0x200;
            } else {
                name_flags = TEXT_ID_UNIT_NAME_GENERIC_MONSTER_BASE;
                name_mod = 0x300;
            }
            do {
                unused_name = 1;
                name_id = name_mod + (rand() * 255) / 0x8000;
                for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
                    party = &g_main_party_data[i];
                    if (party->party_id != PARTY_ID_NONE && gender == (party->gender_flags & NAME_GENDER_MASK)) {
                        name_bytes = party->name_id;
                        if ((name_bytes[0] | (name_bytes[1] << 8)) == (u16)name_id) {
                            unused_name = 0;
                            break;
                        }
                    }
                }
                for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                    other = &g_battle_unit_stats[i];
                    if (gender == (other->unit_flags & NAME_GENDER_MASK) && other->quote_name_id == (u16)name_id) {
                        unused_name = 0;
                        break;
                    }
                }
            } while (unused_name == 0);
            unit->quote_name_id = name_id;
        }
        name_flags += name_id & 0xff;
        text = prepare_text(name_flags);
    } else {
        text = main_party_get_data_pointer(unit->formation_index)->name;
    }
    main_util_copy_bytes(text, unit->name, 16);
    main_util_copy_bytes(prepare_text(unit->job_id | TEXT_ID_JOB_NAME_BASE), unit->job_name, 16);
    main_util_copy_bytes(
        prepare_text(unit->primary_skillset | TEXT_ID_SKILLSET_NAME_BASE), unit->primary_skillset_name, 8);
    main_util_copy_bytes(
        prepare_text(unit->secondary_skillset | TEXT_ID_SKILLSET_NAME_BASE), unit->secondary_skillset_name, 8);
    /* Clear the adjacent inflicted/removed status sets (five bytes each). */
    main_util_clear_byte_data(unit->action.status_infliction, 10);
}

#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"

enum {
    NAME_GENDER_MASK = UNIT_FLAG_MALE | UNIT_FLAG_FEMALE | UNIT_FLAG_MONSTER,
};

/* Copies the unit's name, job name and skillset names into its battle
 * record.  Units without a party slot get a random unused generic name for
 * their gender. */
void main_unit_store_character_names(unit_stats_t* unit) {
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

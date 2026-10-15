#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

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
    mem_copy_byte_data_source_first(&unit->secondary_skillset, &party->secondary_skillset, 0x10);
    party->bravery = unit->original_brave;
    party->faith = unit->original_faith;
    mem_copy_byte_data_source_first(unit->raw_stats, party->raw_stats, sizeof(party->raw_stats));
    /* The saved span includes the career record and following 16-byte name. */
    mem_copy_byte_data_source_first(&unit->career, &party->career, sizeof(party->career) + sizeof(party->name));
    /* name_id is stored as two byte writes (lbu / lhu+srl), not one sh. */
    party->name_id.fields.index = unit->quote_name_id.value;
    party->name_id.fields.name_class = unit->quote_name_id.value >> 8;
    party->proposition_status = 0;
    party->egg_color = 0;
    return 0;
}

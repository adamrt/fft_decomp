#include "fft/main_unit.h"

/*
 * Copies an in-battle unit record into its persistent party slot.
 * Returns 0 on success and -1 when no free party slot exists.
 */
s32 main_party_save_unit(battle_stats_t* unit, s32 allow_guest) {
    party_data_t* party;
    s32 slot;
    s32 index;
    u32 unit_slot;
    s32 kind;
    u8 palette;
    u16 birthday;
    s32 sprite_set;

    kind = unit->unit_flags & UNIT_FLAG_SAVE_FORMATION;
    if (unit->character_identity >= CHARACTER_IDENTITY_SELECTOR_FIRST || allow_guest == 0) {
        kind = 0;
    }
    index = unit->formation_index;
    unit_slot = index & 0xff;

    if ((kind & 0xff) == 0
        && (u32)(index - PARTY_GUEST_SLOT_FIRST) < PARTY_ROSTER_SLOT_COUNT - PARTY_GUEST_SLOT_FIRST) {
        /* A non-guest save of a unit that currently sits in a guest slot. */
        /* The target loads no argument for this one-argument callee. */
        ((void (*)(void))main_party_remove_unit)();
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

    party = main_party_get_data_pointer(slot);
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

    /* The 0x008 halfword packs the birthday (9 bits) and zodiac (top nibble);
     * the target reads it twice, which only a volatile access reproduces. */
    birthday = (((volatile battle_stats_t*)unit)->birthday.value & 0x1ff)
        | (((volatile battle_stats_t*)unit)->birthday.value & 0xf000);
    party->birthday_day = birthday;
    party->zodiac = birthday >> 8;
    main_util_copy_byte_data(&unit->secondary_skillset, &party->secondary_skillset, 0x10);
    party->bravery = unit->original_brave;
    party->faith = unit->original_faith;
    main_util_copy_byte_data(unit->raw_stats, party->raw_stats, 0xf);
    main_util_copy_byte_data(unit->unlocked_jobs, party->unlocked_jobs, 0xa6);
    /* name_id is stored as two byte writes (lbu / lhu+srl), not one sh. */
    party->name_id[0] = (u8)unit->quote_name_id;
    party->name_id[1] = unit->quote_name_id >> 8;
    party->proposition_status = 0;
    party->egg_color = 0;
    return 0;
}

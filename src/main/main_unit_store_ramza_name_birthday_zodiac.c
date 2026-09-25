#include "fft/main.h"

void main_unit_store_ramza_name_birthday_zodiac(battle_stats_t* unit) {
    s32 slot;
    party_data_t* party;
    u32 birthday;

    if (unit->character_identity > CHARACTER_IDENTITY_RAMZA_CHAPTER_4) {
        return;
    }
    for (slot = 0; slot < PARTY_ROSTER_SLOT_COUNT; slot++) {
        party = main_party_get_data_pointer(slot);
        if (party->party_id == PARTY_ID_NONE) {
            continue;
        }
        if (party->sprite_set > CHARACTER_IDENTITY_RAMZA_CHAPTER_4) {
            continue;
        }
        birthday = (party->birthday_day + (party->zodiac << 8)) & PARTY_BIRTHDAY_DAY_MASK;
        unit->birthday.value = (unit->birthday.value & 0xFE00) | birthday;
        unit->birthday.value = (unit->birthday.value & 0xFFF) | ((party->zodiac >> PARTY_ZODIAC_SHIFT) << 12);
        main_util_copy_byte_data(party->name, unit->name, 16);
        return;
    }
}

#include "fft/main.h"
#include "fft/memory.h"

/* For a unit with one of Ramza's three identities, copy the name, birthday and
 * zodiac sign from the first roster record that has one of them. */
void main_unit_store_ramza_name_birthday_zodiac(unit_stats_t* unit) {
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
        mem_copy_byte_data_source_first(party->name, unit->name, sizeof(unit->name));
        return;
    }
}

#include "fft/main_unit.h"
#include "psx/types.h"

s32 require_party_get_free_slot_status(s32 unit_id) {
    party_data_t* party;
    battle_stats_t* unit;
    s32 index;

    unit = battle_unit_get_stats_from_battle_id(unit_id);
    if (unit->unit_flags & UNIT_FLAG_SAVE_FORMATION) {
        return 2;
    }

    index = 0;
    do {
        party = main_party_get_data_pointer(index);
        if ((party->party_id != PARTY_ID_NONE) && (party->sprite_set != 0) && (party->sprite_set < 4)) {
            break;
        }
        index++;
    } while (index < PARTY_ROSTER_SLOT_COUNT);

    return main_party_find_free_slot(0, &party->palette) != -1;
}

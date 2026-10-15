#include "fft/event_require.h"
#include "psx/types.h"

/*
 * Can battle unit `unit_id` join the roster? Returns 2 for a unit with the
 * save-formation flag (a guest), 1 when one of the regular roster slots is
 * free and 0 when they are all taken.
 *
 * main_party_find_free_slot clears the byte it is handed for a palette. Here
 * that is the palette of the first roster record with a Ramza identity (the
 * last record when there is none).
 */
s32 require_party_get_free_slot_status(s32 unit_id) {
    party_unit_t* party;
    unit_stats_t* unit;
    s32 index;

    unit = battle_unit_get_stats_from_battle_id(unit_id);
    if (unit->unit_flags & UNIT_FLAG_SAVE_FORMATION) {
        return 2;
    }

    for (index = 0; index < PARTY_ROSTER_SLOT_COUNT; index++) {
        party = main_party_get_unit(index);
        if ((party->party_id != PARTY_ID_NONE) && (party->sprite_set != CHARACTER_IDENTITY_ENTD_NONE)
            && (party->sprite_set < CHARACTER_IDENTITY_RAMZA_END)) {
            break;
        }
    }

    return main_party_find_free_slot(0, &party->palette) != -1;
}

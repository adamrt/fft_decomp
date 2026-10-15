/* Dismiss, silent join and date advance. */

#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

/* Event instruction: remove the roster unit with sprite set `sprite_set`
 * (slot 0 is never considered), returning its seven equipped items to the
 * inventory. */
void script_dismiss_unit_event_instruction(s32 sprite_set) {
    party_unit_t* party;
    s32 party_index = 1;
    s32 empty_party_id = PARTY_ID_NONE;
    s32 empty_item;
    s32 slot;
    u8 item;

    for (; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party = main_party_get_unit(party_index);
        if (party->party_id != empty_party_id && party->sprite_set == sprite_set) {
            break;
        }
    }

    if (party_index != PARTY_ROSTER_SLOT_COUNT) {
        slot = 0;
        empty_item = ITEM_ID_NONE;
        for (; slot < 7; slot++) {
            item = party->equipment[slot];
            if (item != ITEM_ID_NOTHING && item != empty_item) {
                g_main_item_quantities[item]++;
            }
        }
        main_party_remove_unit(party_index);
    }
}

/* Persist event units marked to join without opening the formation screen. */
void script_join_units_silently(void) {
    s32 out_idx;
    s32 i;
    unit_stats_t* unit;
    u8 first;
    u8 flags;

    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = battle_unit_find_stats_by_entd_unit_id(battle_unit_get_stats_from_battle_id(i)->unit_id, &out_idx);
        if (out_idx >= 0 && i == out_idx) {
            if (unit->formation_index == BATTLE_FORMATION_INDEX_NONE
                || ((first = unit->character_identity) != CHARACTER_IDENTITY_ENTD_NONE
                    && first <= CHARACTER_IDENTITY_RAMZA_CHAPTER_4)) {
                flags = unit->unit_flags;
                if (flags & UNIT_FLAG_JOIN_AFTER_EVENT) {
                    main_party_save_unit(unit, flags & UNIT_FLAG_SAVE_FORMATION);
                }
            }
        }
    }
}

/* CallFunction 2 and 7: stage a 365-day advance in the date-advance variable. The
 * calendar itself moves when that variable is consumed. */
void script_advance_date_by_one_year(void) {
    script_set_variable(EVENT_SCRIPT_VAR_DATE_ADVANCE, 365);
}

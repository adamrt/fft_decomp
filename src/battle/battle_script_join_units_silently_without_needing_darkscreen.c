#include "fft/battle.h"

/* Persist event units marked to join without opening the formation screen. */
void battle_script_join_units_silently_without_needing_darkscreen(void) {
    s32 out_idx;
    s32 i;
    battle_stats_t* unit;
    u8 first;
    u8 flags;

    i = 0;
    do {
        unit = battle_find_unit_data_pointer_for_entd_unit_id(
            battle_unit_get_stats_from_battle_id(i)->unit_id, &out_idx);
        if (out_idx >= 0 && i == out_idx) {
            if (unit->formation_index == BATTLE_FORMATION_INDEX_NONE
                || ((first = unit->character_identity) != CHARACTER_IDENTITY_ENTD_NONE
                    && first <= CHARACTER_IDENTITY_RAMZA_CHAPTER_4)) {
                flags = unit->unit_flags;
                if (flags & UNIT_FLAG_JOIN_AFTER_EVENT) {
                    save_unit_to_party(unit, flags & UNIT_FLAG_SAVE_FORMATION);
                }
            }
        }
        i++;
    } while (i < BATTLE_UNIT_SLOT_COUNT);
}

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Return the unit's position on a freshly built AT list.
 *
 * Returns -1 for an empty slot, -3 for a unit that cannot act and -2 when the
 * unit is not on the list. */
s32 action_find_unit_turn_index(const unit_stats_t* unit) {
    battle_at_entry_t at_list[BATTLE_AT_LIST_CAPACITY];
    s32 i;
    battle_at_entry_t* entry;
    u8 battle_id;

    if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
        return -1;
    if (status_is_unit_absent_dead_crystal_treasure_petrified_or_ridden(unit) != 0)
        return -3;
    battle_id = unit->battle_id;
    action_calculate_at_list(at_list, BATTLE_AT_LIST_MODE_TURNS_ONLY);
    for (i = 0; i < BATTLE_AT_LIST_CAPACITY; i++) {
        entry = &at_list[i];
        if ((entry->unit_code & BATTLE_AT_UNIT_ID_MASK) == (battle_id & 0xFF)) {
            return i;
        }
    }
    return -2;
}

#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Item a fallen unit leaves as treasure: one of its equipped items at random,
 * or with nothing equipped one of the thirteen consumables other than Elixir.
 * Decided only when executing; in any other state the function returns without
 * a value. */
u8 battle_unit_generate_treasure(unit_stats_t* unit) {
    u8 candidates[UNIT_EQUIPMENT_SLOT_COUNT];
    s32 count;
    s32 i;
    u32 item_id;

    if (g_action_state != ACTION_STATE_EXECUTE) {
        return;
    }

    count = 0;
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        candidates[count] = unit->equipment[i];
        item_id = candidates[count];
        if (item_id >= ITEM_ID_END) {
            continue;
        }
        if (item_id == 0) {
            continue;
        }
        count++;
    }

    if (count == 0) {
        candidates[0] = ((rand() * 13) / 0x8000) - (ITEM_ID_COUNT - ITEM_ID_CONSUMABLE_FIRST);
        if (candidates[0] >= ITEM_ID_ELIXIR) {
            candidates[0] = candidates[0] + 1;
        }
        count = 1;
    }

    i = (rand() * count) / 0x8000;
    return candidates[i];
}

/* Count the party's copies of each item, including both inventory and the
 * equipment held by roster members, for the equipment menu's availability. */

#include "fft/equipment.h"
#include "fft/item.h"
#include "fft/party.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/libc.h"
#include "psx/types.h"

/* Snapshot the player inventory counts, then add every item currently equipped
 * by the party roster. */
void item_build_inventory_totals(void) {
    s32 i;
    s32 j;
    party_unit_t* unit;
    u32 item;

    bcopy(g_item_quantities, g_equipment_item_inventory_totals, sizeof(g_item_quantities));

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = party_unit_get(i);
        if (unit->party_id != PARTY_ID_NONE) {
            for (j = 0; j < UNIT_EQUIPMENT_SLOT_COUNT; j++) {
                item = unit->equipment[j];
                if (item != ITEM_ID_NOTHING && item < ITEM_ID_END) {
                    g_equipment_item_inventory_totals[item]++;
                }
            }
        }
    }
}

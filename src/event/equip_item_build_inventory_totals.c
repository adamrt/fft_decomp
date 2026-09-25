#include "fft/event_equip.h"
#include "psx/types.h"

/* Snapshot the player inventory counts, then add every item currently equipped
 * by the party roster. */
void equip_item_build_inventory_totals(void) {
    s32 i;
    s32 j;
    party_data_t* unit;
    u32 item;

    bcopy(g_main_item_quantities, g_equip_item_inventory_totals, 0x100);

    for (i = 0; i < PARTY_ROSTER_SLOT_COUNT; i++) {
        unit = main_party_get_data_pointer(i);
        if (unit->party_id != PARTY_ID_NONE) {
            for (j = 0; j < 7; j++) {
                item = unit->equipment[j];
                if (item != 0 && item < 0xFE) {
                    g_equip_item_inventory_totals[item]++;
                }
            }
        }
    }
}

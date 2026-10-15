#include "fft/event_require.h"
#include "fft/script.h"

/* Return a departing battle unit's seven equipped items to the inventory.
 * An item of which 100 or more are already held or equipped is sold for a
 * quarter of its price instead and removed from the unit. Returns 1 when
 * it sold anything. */
s32 require_party_store_equipment_or_sell_overflow(s32 unit_id) {
    unit_stats_t* unit;
    s32 sold_equipment;
    s32 equipment_slot;

    sold_equipment = 0;
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    equipment_slot = 0;
    do {
        u8 item_id = unit->equipment[equipment_slot];
        s32 sale_value;

        if ((item_id != ITEM_ID_NOTHING) && (item_id != ITEM_ID_NONE)) {
            if (main_item_get_total_equipment_quantity(item_id, 1) >= 100) {
                unit->equipment[equipment_slot] = ITEM_ID_NOTHING;
                sale_value = main_item_get_data_pointer(item_id)->price;
                sale_value /= 4;
                script_set_variable(
                    EVENT_SCRIPT_VAR_WAR_FUNDS, script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + sale_value);
                sold_equipment = 1;
            } else {
                g_main_item_quantities[item_id]++;
            }
        }
        equipment_slot++;
    } while (equipment_slot < 7);
    return sold_equipment;
}

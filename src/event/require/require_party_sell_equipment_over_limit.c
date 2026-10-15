#include "fft/event_require.h"
#include "fft/script.h"

/* Sell each of a battle unit's seven equipped items of which 100 or more are
 * already held or equipped, for a quarter of its price, and remove it from
 * the unit. Returns 1 when it sold anything. */
s32 require_party_sell_equipment_over_limit(s32 unit_id) {
    unit_stats_t* unit;
    s32 sold_equipment;
    s32 equipment_slot;

    sold_equipment = 0;
    unit = battle_unit_get_stats_from_battle_id(unit_id);
    equipment_slot = 0;
    do {
        u8 item_id = unit->equipment[equipment_slot];
        s32 sale_value;

        if (item_id != ITEM_ID_NOTHING && item_id != ITEM_ID_NONE
            && main_item_get_total_equipment_quantity(item_id, 1) >= 100) {
            unit->equipment[equipment_slot] = ITEM_ID_NOTHING;
            sale_value = main_item_get_data_pointer(item_id)->price;
            sale_value /= 4;
            script_set_variable(
                EVENT_SCRIPT_VAR_WAR_FUNDS, script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS) + sale_value);
            sold_equipment = 1;
        }
        equipment_slot++;
    } while (equipment_slot < 7);
    return sold_equipment;
}

/* Formation and shops count inventory and equipped items, read prices and update poached stock. */

#include "fft/formation.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/shop.h"
#include "psx/types.h"

/* Report whether the party has any items: mode 0 asks for inventory or equipped items, mode 1
 * for inventory only, other modes for equipped items only.
 */
s32 formation_inventory_check(s32 mode) {
    s32 has_inventory = 0;
    s32 has_equipment = 0;
    s32 i;
    s32 slot;

    for (i = 1; i < ITEM_ID_END; i++) {
        if (g_item_quantities[i]) {
            has_inventory = 1;
            break;
        }
    }
    for (i = 0; i < g_formation_record_count; i++) {
        for (slot = 0; slot < MENU_EQUIPMENT_SLOT_COUNT; slot++) {
            if (g_formation_unit_pointers[i]->detail.equipment[slot]) {
                has_equipment = 1;
                break;
            }
        }
    }
    if (mode == 0) {
        return has_equipment || has_inventory;
    }
    if (mode == 1) {
        return has_inventory;
    }
    return has_equipment;
}

/* Return an item's price from the item data table; only the item id bits of the entry are used. */
s32 shop_item_price_get(s16 item_id) {
    return g_item_primary_data[item_id & ITEM_ENTRY_ID_MASK].price;
}

/* Return half of the item-table price, clamped to at least 1. */
s32 shop_item_half_price_get(s16 item_id) {
    u32 value = g_item_primary_data[item_id & ITEM_ENTRY_ID_MASK].price >> 1;

    if (value == 0) {
        value = 1;
    }
    return value;
}

/* Count copies of an item equipped across the active formation records.
 *
 * The comparison uses the stored 16-bit equipment value as-is; only the requested
 * ID is masked. */
s32 item_equipped_count_get(s32 item_id) {
    s32 count = 0;
    s32 unit_index;
    s32 slot;
    formation_unit_t* unit;

    item_id &= ITEM_ENTRY_ID_MASK;
    for (unit_index = 0; unit_index < g_formation_record_count; unit_index++) {
        unit = g_formation_unit_pointers[unit_index];
        for (slot = 0; slot < MENU_EQUIPMENT_SLOT_COUNT; slot++) {
            if (unit->detail.equipment[slot] == item_id) {
                count++;
            }
        }
        /* The target reloads the record count on every pass although nothing
         * in the loop stores; without this the load is hoisted. */
        __asm__("" : : : "memory");
    }
    return count;
}

/* Inventory quantity plus the copies equipped by the party. */
s32 item_total_count_get(s16 item_id) {
    item_id &= ITEM_ENTRY_ID_MASK;
    return item_inventory_count_adjust(item_id, 0) + item_equipped_count_get(item_id);
}

/* Add delta to the count of a poached item held for the Fur Shop, keeping it within 0..255, and
 * return the new count; callers pass 0 to read it. */
s32 fur_shop_stock_adjust(s16 item_id, s32 delta) {
    s32 slot = item_id & ITEM_ENTRY_ID_MASK;
    s32 value = g_item_poached_quantities[slot] + delta;

    if (value < 0) {
        value = 0;
    }
    if (value >= 0x100) {
        value = 0xFF;
    }
    g_item_poached_quantities[slot] = value;
    return value;
}

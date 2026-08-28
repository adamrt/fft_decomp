#include "fft/data.h"
#include "fft/world.h"

/* Clears one equipment slot of a formation unit.  An item the unit owned goes
 * back to the fitting room reserve; an item taken from the shop (bit 15 set)
 * is refunded from the fitting room cost instead. */
void world_shop_return_unit_equip_to_fitting_room(s16 unit_index, s16 equipment_index) {
    s32 g_main_item_item_flags = g_world_formation_unit_pointers[unit_index]->equipment[equipment_index];

    if (g_main_item_item_flags != ITEM_ID_NOTHING) {
        if ((g_main_item_item_flags >> 15) == 0)
            g_world_shop_fitting_room_items[unit_index][equipment_index] = g_main_item_item_flags;
        else
            world_shop_add_fitting_room_cost(-world_item_get_price(g_main_item_item_flags & WORLD_ITEM_ID_MASK));
        g_world_formation_unit_pointers[unit_index]->equipment[equipment_index] = ITEM_ID_NOTHING;
    }
}

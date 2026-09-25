#include "fft/world.h"

void world_shop_buy_from_fitting_room(void) {
    s32 i;
    s32 j;

    for (i = 0; i < g_world_formation_unit_count; i++) {
        for (j = 0; j < 5; j++) {
            s32 g_main_item_item_flags = g_world_formation_unit_pointers[i]->equipment[j];
            if ((g_main_item_item_flags >> 14) != 0) {
                g_world_formation_unit_pointers[i]->equipment[j] &= WORLD_ITEM_ID_MASK;
            }
            world_item_change_quantity_on_equip(g_world_shop_fitting_room_items[i][j], 1);
            g_world_shop_fitting_room_items[i][j] = ITEM_ID_NOTHING;
        }
    }
}

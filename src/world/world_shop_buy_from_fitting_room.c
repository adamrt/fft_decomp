#include "fft/gfx.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Complete a fitting-room purchase: every tried-on item (entry bits 14-15
 * set) becomes the unit's own, and the equipment it displaced, kept in
 * g_world_shop_fitting_room_items, goes back to the inventory. */
void world_shop_buy_from_fitting_room(void) {
    s32 i;
    s32 j;

    for (i = 0; i < g_world_formation_unit_count; i++) {
        for (j = 0; j < WORLD_EQUIPMENT_SLOT_COUNT; j++) {
            s32 equipped_item = g_world_unit_pointers[i]->equipment[j];
            if ((equipped_item >> 14) != 0) {
                g_world_unit_pointers[i]->equipment[j] &= WORLD_ITEM_ID_MASK;
            }
            item_adjust_inventory_count(g_world_shop_fitting_room_items[i][j], 1);
            g_world_shop_fitting_room_items[i][j] = ITEM_ID_NOTHING;
        }
    }
}

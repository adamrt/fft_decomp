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

/* Clears one equipment slot of a formation unit.  An item the unit owned goes
 * back to the fitting room reserve; an item taken from the shop (bit 15 set)
 * is refunded from the fitting room cost instead. */
void world_shop_return_unit_equip_to_fitting_room(s16 unit_index, s16 equipment_index) {
    s32 equipped_item = g_world_unit_pointers[unit_index]->equipment[equipment_index];

    if (equipped_item != ITEM_ID_NOTHING) {
        if ((equipped_item >> 15) == 0)
            g_world_shop_fitting_room_items[unit_index][equipment_index] = equipped_item;
        else
            world_shop_add_fitting_room_cost(-world_item_get_price(equipped_item & WORLD_ITEM_ID_MASK));
        g_world_unit_pointers[unit_index]->equipment[equipment_index] = ITEM_ID_NOTHING;
    }
}

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

/* Finalize a unit's equipment from the Fitting Room. */
void world_shop_finalize_unit_equips_from_fitting_room(s16 formation_index, s16 equipment_index) {
    u16 reserved_item;

    g_world_unit_pointers[formation_index]->equipment[equipment_index] = ITEM_ID_NOTHING;
    reserved_item = g_world_shop_fitting_room_items[formation_index][equipment_index];
    if (equipment_index < WORLD_EQUIPMENT_SLOT_HEAD) {
        u16 right_hand = g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
        u16 left_hand = g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND];

        if (right_hand == ITEM_ID_NOTHING && left_hand == ITEM_ID_NOTHING) {
            g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_RIGHT_HAND]
                = g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_RIGHT_HAND];
            g_world_unit_pointers[formation_index]->equipment[WORLD_EQUIPMENT_SLOT_LEFT_HAND]
                = g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_LEFT_HAND];
            g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_RIGHT_HAND] = ITEM_ID_NOTHING;
            g_world_shop_fitting_room_items[formation_index][WORLD_EQUIPMENT_SLOT_LEFT_HAND] = ITEM_ID_NOTHING;
            return;
        }
        if (reserved_item != ITEM_ID_NOTHING
            && world_formation_can_equip_item_in_slot(g_unit_selected_index, equipment_index, reserved_item) == 1) {
            g_world_unit_pointers[formation_index]->equipment[equipment_index] = reserved_item;
            g_world_shop_fitting_room_items[formation_index][equipment_index] = ITEM_ID_NOTHING;
        }
    } else {
        g_world_unit_pointers[formation_index]->equipment[equipment_index] = reserved_item;
        g_world_shop_fitting_room_items[formation_index][equipment_index] = ITEM_ID_NOTHING;
    }
}

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

/* Move every reserved fitting-room item onto its formation unit and clear
 * the reservation. Equipment slots flagged in bit 14 are overwritten even
 * when no item was reserved. */
void world_shop_apply_fitting_room_items_to_all_units(void) {
    s32 i;
    s32 j;

    for (i = 0; i < g_world_formation_unit_count; i++) {
        for (j = 0; j < WORLD_EQUIPMENT_SLOT_COUNT; j++) {
            if ((g_world_unit_pointers[i]->equipment[j] >> 14) != 0
                || g_world_shop_fitting_room_items[i][j] != ITEM_ID_NOTHING) {
                g_world_unit_pointers[i]->equipment[j] = g_world_shop_fitting_room_items[i][j];
            }
            g_world_shop_fitting_room_items[i][j] = ITEM_ID_NOTHING;
        }
    }
}

#include "fft/world.h"

/* Adjust inventory when equipment is installed or removed.
 *
 * Only the low 10 bits select the inventory slot; callers explicitly
 * sign-extend flagged 16-bit entries where the target uses `lh`.
 */
s32 world_item_change_quantity_on_equip(s32 item_id, s32 delta) {
    s32 idx;
    s32 quantity;

    idx = item_id & WORLD_ITEM_ID_MASK;
    quantity = 0;
    if (idx != ITEM_ID_NOTHING) {
        quantity = g_main_item_quantities[idx] + delta;
        if (quantity < 0) {
            quantity = 0;
        }
        g_main_item_quantities[idx] = (u8)quantity;
    }
    return quantity;
}

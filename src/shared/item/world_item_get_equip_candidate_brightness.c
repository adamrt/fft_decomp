#include "fft/world.h"
#include "psx/types.h"

/* Menu callback: sprite brightness for a unit on the shop's "who can equip"
 * view: 0x80 when the unit can equip the candidate item, 0x40 when not. */
s32 world_item_get_equip_candidate_brightness(s32 unit_index) {
    return world_unit_can_equip_item_id(unit_index, g_world_shop_equip_candidate_item) == 1 ? 0x80 : 0x40;
}

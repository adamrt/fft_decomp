#include "fft/world.h"
#include "psx/types.h"

s32 world_item_get_equip_candidate_brightness(s32 unit_index) {
    return world_unit_can_equip_item_id((s16)unit_index, g_world_shop_equip_candidate_item) == 1 ? 0x80 : 0x40;
}

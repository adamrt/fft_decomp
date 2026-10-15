#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* No retail code calls it. */
void battle_gfx_set_thrown_item_graphic_palette_by_battle_id(s32 item_id, u16 battle_id) {
    battle_gfx_set_thrown_item_graphic_palette(item_id, battle_unit_get_by_battle_id(battle_id));
}

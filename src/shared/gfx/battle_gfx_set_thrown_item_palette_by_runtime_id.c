#include "fft/battle_gfx.h"

/* TempWeapon event instruction: battle_gfx_set_thrown_item_graphic_palette for a
 * runtime id. An absent unit is skipped. */
void battle_gfx_set_thrown_item_palette_by_runtime_id(u32 item_id, u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit != 0) {
        battle_gfx_set_thrown_item_graphic_palette(item_id, unit);
    }
}

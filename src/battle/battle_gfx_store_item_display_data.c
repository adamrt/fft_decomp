#include "fft/battle.h"
#include "fft/battle_gfx.h"

/* Make the unit's item display show an item's 16x16 icon, centerd on the
 * display's origin. */
void battle_gfx_store_item_display_data(battle_unit_t* unit, u32 item_id) {
    SPRT data;

    battle_gfx_set_item_icon_sprite(&data, item_id);
    unit->item_display->clut = data.clut;
    battle_gfx_store_sprite_display_data(
        unit->item_display, 0, (16 - data.w) / 2 - 8, (16 - data.h) / 2 - 8, data.u0, data.v0, data.w, data.h, 0);
}

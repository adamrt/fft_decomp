#include "fft/battle.h"

void battle_gfx_set_thrown_item_palette_by_misc_id(u32 item_id, u32 misc_id) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff);
    if (unit != 0) {
        battle_gfx_set_thrown_item_graphic_palette(item_id, unit);
    }
}

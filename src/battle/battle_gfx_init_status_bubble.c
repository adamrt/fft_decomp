#include "fft/battle.h"

void battle_gfx_init_status_bubble(battle_unit_misc_data_t* unit) {
    u8 tile_offset;

    tile_offset = unit->centre_tile_offset;
    if (tile_offset != 0x2D && tile_offset != 0x31 && tile_offset != 0x39 && tile_offset != 0x35) {
        battle_unit_increment_or_decrement_height_mod(unit);
    }
    if (unit->status_bubble_active != 0) {
        battle_gfx_determine_status_bubble_parameters(unit);
    }
}

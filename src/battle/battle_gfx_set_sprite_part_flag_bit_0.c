#include "fft/battle.h"

void battle_gfx_set_sprite_part_flag_bit_0(battle_gfx_sprite_display_data_t* display, s32 part_index, s32 enabled) {
    if (enabled != 0) {
        display->parts[part_index].flags |= 1;
    } else {
        display->parts[part_index].flags &= 0xfe;
    }
}

#include "fft/battle.h"

void battle_gfx_shift_and_fill_display_svectors(battle_gfx_render_unit_t* unit) {
    s32 index;

    index = 5;
    do {
        unit->display_svectors[index] = unit->display_svectors[index - 1];
        index--;
    } while (index > 0);
    battle_unit_copy_misc_data_xyz_values(unit->display_svectors, (battle_unit_misc_data_t*)unit);
}

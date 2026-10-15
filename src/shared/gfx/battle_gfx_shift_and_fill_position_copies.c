#include "fft/battle_gfx.h"

/* Advances the unit's position trail: each copy takes the next newer one and copy
 * 0 the unit's current position. */
void battle_gfx_shift_and_fill_position_copies(battle_unit_t* unit) {
    s32 index;

    for (index = 5; index > 0; index--) {
        unit->position_copies[index] = unit->position_copies[index - 1];
    }
    battle_unit_copy_draw_coords(unit->position_copies, unit);
}

#include "fft/battle_gfx.h"

/* Enables the unit's six-copy position trail and seeds every copy with its
 * current position. */
void battle_gfx_init_position_vector_copies(battle_unit_t* unit) {
    s32 i;

    unit->position_copies_active = 1;
    for (i = 0; i < 6; i++) {
        main_util_set_svector(
            &unit->position_copies[i], unit->world_coords.vx, unit->world_coords.vy, unit->world_coords.vz);
    }
}

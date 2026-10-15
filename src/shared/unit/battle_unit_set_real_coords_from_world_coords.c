#include "fft/battle_unit.h"

/* Sets the 20.12 fixed-point position from the integer world position. */
void battle_unit_set_real_coords_from_world_coords(battle_unit_t* unit) {
    unit->real.vx = unit->world_coords.vx * ONE;
    unit->real.vy = unit->world_coords.vy * ONE;
    unit->real.vz = unit->world_coords.vz * ONE;
}

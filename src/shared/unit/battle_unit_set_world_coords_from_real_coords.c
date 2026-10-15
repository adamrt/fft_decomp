#include "fft/battle_unit.h"

/* Sets the integer world position from the 20.12 fixed-point position. */
void battle_unit_set_world_coords_from_real_coords(battle_unit_t* unit) {
    unit->world_coords.vx = unit->real.vx / ONE;
    unit->world_coords.vy = unit->real.vy / ONE;
    unit->world_coords.vz = unit->real.vz / ONE;
}

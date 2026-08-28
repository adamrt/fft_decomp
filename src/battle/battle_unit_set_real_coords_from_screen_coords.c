#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_set_real_coords_from_screen_coords(battle_unit_misc_data_t* unit) {
    unit->real.vx = unit->screen.vx << 12;
    unit->real.vy = unit->screen.vy << 12;
    unit->real.vz = unit->screen.vz << 12;
}

#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_set_screen_coords_from_real_coords(battle_unit_misc_data_t* misc) {
    misc->screen.vx = misc->real.vx / ONE;
    misc->screen.vy = misc->real.vy / ONE;
    misc->screen.vz = misc->real.vz / ONE;
}

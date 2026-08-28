#include "fft/battle.h"
#include "psx/types.h"

s32 battle_unit_build_deployed_units_data(s32 formation) {
    return battle_unit_init_deployed_units_data((battle_deployed_coords_t*)formation, 0);
}

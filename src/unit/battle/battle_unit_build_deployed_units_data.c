#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/types.h"

/* battle_unit_init_deployed_units_data for a normal deployment. The second
 * argument is ignored. */
s32 battle_unit_build_deployed_units_data(unit_deployment_coordinates_t* formation, s32 unused_1) {
    return battle_unit_init_deployed_units_data(formation, 0);
}

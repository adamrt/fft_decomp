#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

void battle_gfx_init_position_vector_copies(battle_unit_t* unit) {
    s32 i;

    unit->position_copies_active = 1;
    for (i = 0; i < 6; i++) {
        main_util_set_svector(
            &unit->position_copies[i], unit->world_coords.vx, unit->world_coords.vy, unit->world_coords.vz);
    }
}

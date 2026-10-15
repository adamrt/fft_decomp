#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* The first record in the runtime unit list standing on a tile, whatever its state
 * (crystals, treasures and records without battle stats included), or 0. */
battle_unit_t* battle_unit_find_runtime_at_tile(s32 map_x, s32 map_y, s32 map_z) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        if (unit->map_x == map_x && unit->map_y == map_y && unit->map_z == map_z) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}

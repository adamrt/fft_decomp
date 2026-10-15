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

/* The runtime (display) record whose battle stats occupy unit slot battle_id, or 0. */
battle_unit_t* battle_unit_get_by_battle_id(u16 battle_id) {
    battle_unit_t* unit;

    unit = g_battle_unit_list_head;
    while (unit != 0) {
        if ((unit->stats != 0) && (unit->stats->battle_id == battle_id)) {
            return unit;
        }
        unit = unit->previous;
    }
    return 0;
}

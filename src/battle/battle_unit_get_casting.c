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

battle_unit_t* battle_unit_get_casting(void) {
    battle_unit_t* unit;

    for (unit = g_battle_unit_list_head; unit != 0; unit = unit->previous) {
        if (unit->runtime_id == g_battle_casting_runtime_id) {
            return unit;
        }
    }
    return 0;
}

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* A rider's mount; any other unit is returned itself. */
battle_unit_t* battle_unit_get_mount(battle_unit_t* unit) {
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        return battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
    }
    return unit;
}

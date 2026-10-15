#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Hides a unit's shadow (UnitShadow event instruction). Returns 0 when no unit
 * has the runtime id. */
s32 battle_gfx_clear_shadow_graphic_trigger_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->shadow_graphic_trigger = 0;
        return 1;
    }
    return 0;
}

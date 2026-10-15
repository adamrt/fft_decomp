#include "fft/battle_gfx.h"

/* Hides a unit: clears draw_enabled and its sprite graphic trigger. Returns 0
 * when no unit has the runtime id. */
s32 battle_gfx_reset_unit_graphic_trigger(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);

    if (unit != 0) {
        unit->draw_enabled = 0;
        unit->animation.trigger = 0;
        return 1;
    }
    return 0;
}

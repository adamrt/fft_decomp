#include "fft/battle_gfx.h"

/* Rebuilds the casting unit's sprite parts for its current animation and frame. */
void battle_gfx_load_casting_unit_graphics(void) {
    battle_unit_t* unit;

    unit = battle_unit_get_casting();
    if (unit != 0) {
        /* The sprite data block starts at the graphic trigger; 0x1e0 is the
         * current frame. */
        battle_gfx_load_trap_and_unit_frame_parts(
            unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    }
}

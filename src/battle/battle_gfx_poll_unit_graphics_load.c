#include "fft/battle.h"

/* The underlying loader handles ordinary SPR as well as special SHP/SEQ.
 * Keep polling only its immediate-progress result, not deferred work. */
s32 battle_gfx_poll_unit_graphics_load(void) {
    s32 status;

    do {
        status = battle_gfx_step_queued_unit_graphics_load();
    } while (status == 2);
    return status;
}

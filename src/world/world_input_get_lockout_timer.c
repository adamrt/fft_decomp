#include "fft/world.h"
#include "psx/types.h"

/* Countdown maintained by 0x80113580; while it is non-zero the formation
   screen's per-frame handler returns before reading controller input. */
s32 world_input_get_lockout_timer(void) {
    return g_world_input_lockout_timer;
}

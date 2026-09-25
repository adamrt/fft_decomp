#include "fft/event_bunit.h"
#include "psx/types.h"

s32 bunit_cmd_advance_transition_frame_handler(s32 value) {
    if (g_bunit_gfx_transition_frame < 4) {
        g_bunit_gfx_transition_frame++;
    }
    return value + 1;
}

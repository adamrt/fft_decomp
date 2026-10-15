#include "fft/world.h"
#include "psx/types.h"

/* Returns the current window open/close scaling animation step. */
s32 world_menu_get_window_scale_step(void) {
    return g_gfx_transition_frame;
}

/* The current frame of the window open and close scaling transition. */

#include "fft/menu.h"
#include "psx/types.h"

/* Return the current frame of the window open/close scaling transition. */
s32 menu_get_window_scale_step(void) {
    return g_gfx_transition_frame;
}

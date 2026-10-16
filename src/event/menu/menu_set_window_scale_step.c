#include "fft/menu.h"
#include "psx/types.h"

/* Set the current frame of the window open/close scaling transition. */
void menu_set_window_scale_step(s32 value) {
    g_gfx_transition_frame = value;
}

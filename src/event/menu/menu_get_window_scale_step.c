#include "fft/menu.h"
#include "psx/types.h"

s32 menu_get_window_scale_step(void) {
    return g_gfx_transition_frame;
}

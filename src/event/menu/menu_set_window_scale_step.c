#include "fft/menu.h"
#include "psx/types.h"

void menu_set_window_scale_step(s32 value) {
    g_gfx_transition_frame = value;
}

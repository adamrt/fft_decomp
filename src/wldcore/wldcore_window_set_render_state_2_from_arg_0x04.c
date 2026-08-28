#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_window_set_render_state_2_from_arg_0x04(s32* param) {
    g_wldcore_window_render_records[param[1]].palette = 2;
}

#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_window_clear_pair_state(s32* param) {
    s32 first = param[1];
    s32 second = param[0];

    g_wldcore_window_records[first].palette = 0;
    g_wldcore_window_records[second].palette = 0;
}

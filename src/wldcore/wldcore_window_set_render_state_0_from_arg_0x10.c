#include "fft/wldcore.h"
#include "psx/types.h"

/* Sets the level's render record (index at word 4, -1 when absent) to render
 * state 0. */
void wldcore_window_set_render_state_0_from_arg_0x10(s32* level) {
    s32 index;

    index = level[4];
    if (index != -1) {
        g_wldcore_window_render_records[index].palette = 0;
    }
}

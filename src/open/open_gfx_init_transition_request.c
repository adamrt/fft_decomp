#include "fft/open.h"
#include "psx/types.h"

void open_gfx_init_transition_request(s32 target, s32 mode, open_gfx_transition_entry_t** entry_table) {
    g_open_gfx_transition.flags = 1;
    g_open_gfx_transition.current_index = 0;
    g_open_gfx_transition.target_index = target;
    g_open_gfx_transition.entry_table = entry_table;
    g_open_gfx_transition.mode = mode;
    g_open_gfx_transition.intensity = 0x80;
}

#include "fft/open.h"
#include "psx/types.h"

/* Request the new-game prologue over entry_table: entries 0 to target - 1 run,
 * with their text in ordering-table entries ot_layer and ot_layer + 1. */
void open_gfx_init_transition_request(s32 target, s32 ot_layer, open_gfx_transition_entry_t** entry_table) {
    g_open_gfx_transition.flags = OPEN_GFX_TRANSITION_NEXT_ENTRY;
    g_open_gfx_transition.current_index = 0;
    g_open_gfx_transition.target_index = target;
    g_open_gfx_transition.entry_table = entry_table;
    g_open_gfx_transition.ot_layer = ot_layer;
    g_open_gfx_transition.intensity = 0x80;
}

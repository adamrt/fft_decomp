/* No shared theme; frame otag/input update plus the map-destruction wait state advance. */

#include "fft/gfx.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "psx/types.h"

void script_set_current_otag_entry_and_update_input(u32* entry, s32 input) {
    script_update_event_input_state(input);
    g_gfx_current_otag_entry = entry;
    menu_draw_active_window_frames_variant();
}

/* Advance the map-destruction wait state from 1 (armed when a map append
 * finishes) to 2. script_process_pending_requests_variant then releases the held
 * GNS data and polls the map load, returning the state to 0 when the load ends.
 * Any other state is left alone. */
void script_set_specialized_map_destroyed(void) {

    if (g_map_destruction_wait_state == 1) {
        g_map_destruction_wait_state = g_map_destruction_wait_state + 1;
    }
}

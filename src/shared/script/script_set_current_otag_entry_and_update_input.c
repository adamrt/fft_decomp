#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "psx/types.h"

/* Update the event input state from `input`, make `entry` the active ordering
 * table and draw the open window frames. */
void script_set_current_otag_entry_and_update_input(u32* entry, s32 input) {
    script_update_event_input_state(input);
    g_gfx_current_otag_entry = entry;
    menu_draw_active_window_frames_variant();
}

#include "fft/open.h"
#include "psx/types.h"

/* While OPEN_SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME is set (the music test), run
 * one WORLD menu frame with the blocked buttons withheld, drawing from entry
 * g_open_menu_formation_otag_index of the ordering table OPEN is building. The
 * active buffer index is flipped for the duration of the call. */
void open_menu_update_world_formation(void) {
    if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_WORLD_MENU_FRAME) {
        u32 blocked_buttons = g_menu_blocked_buttons;

        g_active_graphics_buffer_index ^= 1;
        world_formation_set_menu_allowed_buttons(~blocked_buttons);
        world_formation_run_menu_frame(
            &g_open_gfx_otags[g_active_graphics_buffer_index ^ 1][g_open_menu_formation_otag_index],
            g_input_current_buttons);
        g_active_graphics_buffer_index ^= 1;
    }
}

#include "fft/main_runtime.h"
#include "fft/open.h"
#include "fft/world.h"
#include "psx/types.h"

void open_menu_update_world_formation(void) {
    if (g_open_system_runtime_flags & 0x400) {
        u32 entry_mask = g_open_menu_formation_entry_mask;

        g_active_graphics_buffer_index ^= 1;
        world_formation_set_menu_entry_mask(~entry_mask);
        world_formation_run_menu_frame(
            &g_open_gfx_otags[g_active_graphics_buffer_index ^ 1][g_open_menu_formation_otag_index],
            g_open_input_current_buttons);
        g_active_graphics_buffer_index ^= 1;
    }
}

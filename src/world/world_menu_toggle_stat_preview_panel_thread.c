#include "fft/world.h"
#include "psx/types.h"

/* Starts thread 9 on world_menu_equipment_panel_thread (parameter g_world_stat_preview_panel_thread_params) when
 * `enable` is set and neither of the g_world_status_display_thread_params.flags 0x60 bits is set; stops it otherwise.
 * g_world_change_banner_panel_thread_params.style records the normalized enable state. */
void world_menu_toggle_stat_preview_panel_thread(s32 enable) {
    if (enable != 0) {
        if (!(g_world_status_display_thread_params.flags & 0x60) && world_thread_is_running(9) == 0) {
            world_thread_start(9, world_menu_equipment_panel_thread);
            world_thread_set_parameters(9, (s32)&g_world_stat_preview_panel_thread_params, 0, 0);
        }
        enable = 1;
    } else {
        world_menu_stop_unit_status_banner_thread(9);
    }
    g_world_change_banner_panel_thread_params.style = enable;
}

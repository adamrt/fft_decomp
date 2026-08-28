#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Start thread 9 on world_menu_equipment_and_ability_panel_thread unless threads 9/10 already run. */
void world_menu_start_equipment_and_ability_panel_thread(s32 enabled) {
    if (enabled == 0) {
        return;
    }
    enabled = 1;
    if (g_world_status_display_thread_params.flags & 0x60) {
        return;
    }
    if (world_thread_is_running(9) == 0 && world_thread_is_running(10) == 0) {
        world_thread_start(9, world_menu_equipment_and_ability_panel_thread);
        world_thread_set_parameters(9, (s32)&g_world_equipment_ability_panel_thread_params, 0, 0);
    }
    world_menu_toggle_numeric_editor_thread(enabled);
}

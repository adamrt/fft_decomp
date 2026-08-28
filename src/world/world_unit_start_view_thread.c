#include "fft/world.h"
#include "psx/types.h"

void world_unit_start_view_thread(s32 index, s32 unit_a, s32 unit_b) {
    if (*(u16*)&g_world_menu_overlay_state != 1) {
        world_text_generate_battle_unit_name_string();
        g_world_menu_panel_fade_mode = g_world_unit_view_panel_fade_modes[index];
        if (unit_a != 0xFF) {
            g_world_unit_view_battle_id = unit_a;
            g_world_unit_selected_status_billboard.battle_id = unit_a;
        }
        if (unit_b != 0xFF) {
            g_world_unit_comparison_battle_id = unit_b;
            g_world_unit_comparison_status_billboard.battle_id = unit_b;
        }
        if (world_thread_is_running_80100164(2) == 0) {
            world_thread_start(2, world_unit_view_supervisor_thread);
        }
        world_thread_set_parameters(2, index, 0, 0);
    }
}

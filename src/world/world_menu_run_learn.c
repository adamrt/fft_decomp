#include "fft/world.h"
#include "psx/types.h"

/* Learn menu processing: opens the menu on first entry, then runs either
 * the skillset list (state 0) or the ability list (state 1). Returns 0
 * once the menu has been closed. */
s32 world_menu_run_learn(void) {
    s32 result;

    if (g_world_learn_menu_initialized == 0) {
        g_world_learn_menu_step = 0;
        g_world_formation_unit_banner_enabled = 0;
        world_menu_stop_unit_status_banner_thread(12);
        world_menu_stop_unit_status_banner_thread(10);
        g_world_formation_unit_cycle_mode = 0;
        world_menu_set_cursor_position_2(8, 0);
        world_gfx_clear_sprite_slot(9);
        g_world_learn_menu_initialized = 1;
    }
    if (g_world_learn_menu_step == 0) {
        result = world_menu_run_learn_job_list();
        if (result == 1) {
            world_menu_init_scrollable_list_core(0, 0, 0);
            g_world_learn_menu_step = 1;
        } else if (result == -1) {
            g_world_learn_menu_initialized = 0;
            g_world_formation_unit_banner_enabled = 1;
            world_formation_save_records_to_party_data();
            return 0;
        }
    } else {
        g_world_learn_menu_step = world_ability_run_learn_list();
    }
    return 1;
}

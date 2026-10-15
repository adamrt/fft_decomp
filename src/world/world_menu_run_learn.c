#include "fft/world.h"
#include "psx/types.h"

/* Learn menu processing: opens the menu on first entry, then runs either
 * the skillset list (state 0) or the ability list (state 1). Returns 0
 * once the menu has been closed. */
s32 world_menu_run_learn(void) {
    s32 result;

    if (g_world_learn_menu_initialized == 0) {
        g_world_learn_menu_step = MENU_JOB_LIST_STEP_JOBS;
        g_world_formation_unit_banner_enabled = 0;
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
        menu_request_thread_stop(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL);
        g_world_formation_unit_cycle_mode = 0;
        world_menu_init_cursor_position(WORLD_MENU_CURSOR_ABILITY_TAB, 0);
        menu_clear_selection_record(9);
        g_world_learn_menu_initialized = 1;
    }
    if (g_world_learn_menu_step == MENU_JOB_LIST_STEP_JOBS) {
        result = world_menu_run_learn_job_list();
        if (result == MENU_JOB_LIST_STEP_ABILITIES) {
            menu_init_scrollable_list_core_variant(0, 0, 0);
            g_world_learn_menu_step = MENU_JOB_LIST_STEP_ABILITIES;
        } else if (result == MENU_JOB_LIST_STEP_CLOSE) {
            g_world_learn_menu_initialized = 0;
            g_world_formation_unit_banner_enabled = 1;
            world_formation_save_records_to_party();
            return 0;
        }
    } else {
        g_world_learn_menu_step = world_ability_run_learn_list();
    }
    return 1;
}

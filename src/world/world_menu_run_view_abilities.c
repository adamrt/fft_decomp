#include "fft/world.h"

/* Menu processing sibling of world_menu_run_learn: opens on first entry, runs
 * sub-state 0 (world_menu_run_learn_job_list) or 1 (world_ability_run_view_list), tears down on -1. */
void world_menu_run_view_abilities(void) {
    if (g_world_view_abilities_initialized == 0) {
        g_world_view_abilities_initialized = 1;
        g_world_formation_unit_cycle_mode = 1;
        g_world_view_abilities_step = MENU_JOB_LIST_STEP_JOBS;
        g_menu_status_display_thread_params.style = 1;
        g_menu_numeric_panel_params.style = 1;
        g_world_equipment_ability_panel_thread_params.style = 1;
        world_menu_init_cursor_position(WORLD_MENU_CURSOR_ABILITY_TAB, 0);
        menu_clear_selection_records();
        g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
    }
    if (g_world_view_abilities_step == MENU_JOB_LIST_STEP_JOBS) {
        g_world_view_abilities_step = world_menu_run_learn_job_list();
    } else if (g_world_view_abilities_step == MENU_JOB_LIST_STEP_ABILITIES) {
        g_world_view_abilities_step = world_ability_run_view_list();
    }
    if (g_world_view_abilities_step == MENU_JOB_LIST_STEP_CLOSE) {
        g_menu_numeric_panel_params.style = 0;
        g_world_equipment_ability_panel_thread_params.style = 0;
        g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
        g_world_formation_unit_cycle_mode = 2;
        menu_input_clear_state();
        g_world_input_secondary_repeat_latched = 0;
        g_menu_status_display_thread_params.style = 0;
        g_world_view_abilities_initialized = 0;
    }
}

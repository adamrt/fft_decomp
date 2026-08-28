#include "fft/world.h"
#include "psx/types.h"

void world_formation_init_menu_state(void) {
    g_world_formation_main_menu_initialized = 0;
    g_world_item_menu_initialized = 0;
    g_world_equip_menu_initialized = 0;
    g_world_item_best_remove_initialized = 0;
    g_world_item_submenu_initialized = 0;
    g_world_item_category_menu_initialized = 0;
    g_world_item_rearrange_initialized = 0;
    g_world_item_action_menu_initialized = 0;
    g_world_item_sort_order_initialized = 0;
    g_world_item_equipping_units_initialized = 0;
    g_world_ability_menu_initialized = 0;
    g_world_remove_ability_initialized = 0;
    g_world_set_ability_initialized = 0;
    g_world_learn_menu_initialized = 0;
    g_world_job_wheel_initialized = 0;
    g_world_learn_job_list_initialized = 0;
    g_world_learn_ability_list_open = 0;
    g_world_view_abilities_initialized = 0;
    g_world_ability_view_list_initialized = 0;
    g_world_dismiss_unit_prompt_running = 0;
    g_world_menu_order_unit_open = 0;
    /* Through the records rather than the member scalars (offset_y, flags,
     * highlight): the target addresses these stores from one base each. */
    g_world_status_display_thread_params.y = 0x90;
    g_world_status_display_thread_params.flags = 0;
    g_world_status_display_thread_params.style = 0;
    g_world_comparison_display_thread_params.style = 0;
    g_world_change_banner_panel_thread_params.style = 0;
    g_world_stat_preview_panel_thread_params.style = 0;
    g_world_ability_panel_thread_params.style = 0;
    g_world_equipment_ability_panel_thread_params.style = 0;
    g_world_numeric_editor_thread_params.style = 0;
    g_world_preview_stats_thread_params.style = 0;
    g_world_preview_stats_thread_params.y = 2;
    g_world_formation_triangle_menu.cursor = 0;
    g_world_formation_current_menu = 0;
    g_world_formation_selected_unit_index = 0;
    g_world_formation_selected_unit_index_latch = 0;
    g_world_formation_unit_cycle_mode = 1;
    g_world_formation_unit_banner_enabled = 1;
    g_world_formation_scroll_enabled_latch = 1;
    g_world_formation_scroll_enabled = 1;
    g_world_formation_unit_browse_enabled = 0;
    g_world_formation_cursor_ot_override = 0;
    g_world_formation_triangle_menu_open = 0;
    world_formation_init_menu_display();
    world_formation_rebuild_unit_list();
}

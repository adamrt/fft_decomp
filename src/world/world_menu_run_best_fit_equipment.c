#include "fft/world.h"

/*
 * Run the "best fit" equipment menu: build the suggested set once, then run
 * the confirmation thread. On confirm, return the current equipment to the
 * inventory and equip the suggested set.
 */
s32 world_menu_run_best_fit_equipment(void) {
    /* The target reserves 64 local bytes whose contents are not accessed. */
    s32 unused[16];
    s32 i;
    s16* choice;
    world_formation_unit_t* unit;

    if (g_world_item_best_remove_initialized == 0) {
        g_world_best_fit_confirm_menu.cursor = 0;
        world_unit_build_best_fit_equipment(
            g_world_formation_selected_unit_index, (s16*)g_world_item_preview_stat_detail.equipment);
        world_item_calculate_equipment_swap_stat_delta(&g_world_item_preview_stat_detail,
            &g_world_selected_unit_stat_summary,
            g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment,
            g_world_item_preview_stat_detail.equipment);
        g_world_item_preview_stat_detail.two_hands
            = world_item_check_two_hands_for_weapons((struct weapon_pair*)g_world_item_preview_stat_detail.equipment,
                world_ability_has_two_hands(g_world_formation_selected_unit_index));
        world_menu_toggle_preview_stats_window(1);
        world_menu_toggle_stat_preview_panel_thread(1);
        g_world_preview_stats_thread_params.redraw_request = 1;
        g_world_formation_panel_windows[0].enabled = 1;
        world_thread_suspend(0xF);
        g_world_formation_unit_cycle_mode = 1;
        g_world_item_best_remove_initialized = 1;
        g_world_menu_sound_muted = 1;
    }
    g_world_item_best_remove_initialized = world_menu_run_thread(6, &g_world_best_fit_confirm_menu);
    if (g_world_item_best_remove_initialized == 0) {
        choice = &g_world_menu_selection_results[11];
        if (*choice == 0) {
            g_world_menu_sound_effect_id = MAIN_SFX_EQUIP;
        }
        if (*choice == 1) {
            g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
        }
        world_menu_toggle_stat_preview_panel_thread(0);
        world_menu_toggle_preview_stats_window(0);
        g_world_formation_panel_windows[0].enabled = 1;
        g_world_formation_unit_cycle_mode = 2;
        if (*choice == 0) {
            for (i = 0; i < 5; i++) {
                world_item_change_quantity_on_equip(
                    (s16)g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[i], 1);
            }
            for (i = 0; i < 5; i++) {
                g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->equipment[i]
                    = g_world_item_preview_stat_detail.equipment[i];
                world_item_change_quantity_on_equip((s16)g_world_item_preview_stat_detail.equipment[i], -1);
            }
            world_formation_save_records_to_party_data();
            world_formation_stage_selected_unit();
        }
        world_thread_resume(0xF);
        g_world_menu_sound_muted = 0;
        return 0;
    }
    return 1;
}

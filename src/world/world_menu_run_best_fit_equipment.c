#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
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

    if (g_world_item_best_remove_initialized == 0) {
        g_world_best_fit_confirm_menu.selected_index = 0;
        world_unit_build_best_fit_equipment(g_unit_selected_index, g_item_preview_stat_detail.state.equipment);
        item_calculate_equipment_swap_stat_delta(&g_item_preview_stat_detail, &g_menu_selected_unit_stat_summary,
            g_world_unit_pointers[g_unit_selected_index]->equipment, (u16*)g_item_preview_stat_detail.state.equipment);
        g_item_preview_stat_detail.state.two_hands = unit_is_two_handing_weapon(
            g_item_preview_stat_detail.state.equipment, world_ability_has_two_hands(g_unit_selected_index));
        world_menu_toggle_preview_stats_window(1);
        menu_toggle_stat_preview_panel_thread(1);
        g_menu_preview_stats_thread_params.redraw_request = 1;
        g_menu_status_display_thread_params.redraw_request = 1;
        thread_suspend(WORLD_THREAD_SLOT_UPPER_MENU);
        g_world_formation_unit_cycle_mode = 1;
        g_world_item_best_remove_initialized = 1;
        g_sound_suppressed = 1;
    }
    g_world_item_best_remove_initialized
        = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_best_fit_confirm_menu);
    if (g_world_item_best_remove_initialized == 0) {
        choice = &g_world_menu_selection_results[11];
        if (*choice == 0) {
            g_sound_queued_effect_id = MAIN_SFX_EQUIP;
        }
        if (*choice == 1) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
        }
        menu_toggle_stat_preview_panel_thread(0);
        world_menu_toggle_preview_stats_window(0);
        g_menu_status_display_thread_params.redraw_request = 1;
        g_world_formation_unit_cycle_mode = 2;
        if (*choice == 0) {
            for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                item_adjust_inventory_count(g_world_unit_pointers[g_unit_selected_index]->equipment[i], 1);
            }
            for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
                g_world_unit_pointers[g_unit_selected_index]->equipment[i]
                    = g_item_preview_stat_detail.state.equipment[i];
                item_adjust_inventory_count(g_item_preview_stat_detail.state.equipment[i], -1);
            }
            world_formation_save_records_to_party();
            world_formation_stage_selected_unit();
        }
        thread_resume(WORLD_THREAD_SLOT_UPPER_MENU);
        g_sound_suppressed = 0;
        return 0;
    }
    return 1;
}

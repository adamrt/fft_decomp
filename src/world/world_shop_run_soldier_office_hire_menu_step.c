#include "fft/world.h"
#include "psx/pad.h"

/* Soldier Office hire step: runs the hire menu and charges for the new
 * unit's starting equipment. */
void world_shop_run_soldier_office_hire_menu_step(void) {
    battle_stats_t unit;
    s32 i;
    s32 result;
    s32 slot;

    if (g_world_shop_hire_menu_initialized == 0) {
        i = 0;
        g_world_formation_dismiss_final_confirm_menu.cursor = 0;
        g_world_shop_rename_confirm_menu.cursor = 0;
        g_world_shop_hire_result_shown = 0;
        g_world_shop_hire_menu_initialized = 1;
        g_world_status_display_thread_params.y = 0;
        D_801CD0EC = 0;
        D_801CD0F4 = 0;
        g_world_formation_unit_banner_enabled = 0;
        g_world_shop_party_unit_count = 0;
        do {
            result = main_unit_init_job_data(&unit, i, 1);
            i++;
            if (result == 0 && unit.formation_index < PARTY_GUEST_SLOT_FIRST) {
                g_world_shop_party_unit_count++;
            }
        } while (i < 20);
    }
    if (g_world_shop_hire_result_shown != 0) {
        if (g_world_input_primary_repeat & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE)) {
            g_world_shop_menu_step = 0x19;
        }
    } else {
        i = world_menu_run_thread(0xF, &g_world_shop_hire_menu_thread_data);
        if (i == 0) {
            if (g_world_menu_selection_results[0] == 3 || g_world_menu_selection_results[0] == -1) {
                g_world_shop_hire_menu_initialized = 0;
                g_world_shop_menu_step = 0xD;
                world_thread_set_parameters(2, 0x19, 0xF825, 0);
                world_item_cash_out_excess_inventory();
            } else if (g_world_menu_selection_results[0] == 2) {
                g_world_formation_unit_count = world_formation_build_record_list(0, g_world_formation_unit_pointers, 2);
                if (g_world_formation_unit_count != 0) {
                    g_world_shop_menu_step = 0x1A;
                    i = 0xF847;
                } else {
                    i = 0xF846;
                    g_world_shop_hire_result_shown = 1;
                }
                world_thread_set_parameters(2, 0x19, i, 0);
            } else {
                slot = world_formation_generate_unit(g_world_menu_selection_results[0]);
                if (slot >= 0) {
                    g_world_shop_service_fee = 1000;
                    do {
                        g_world_shop_service_fee
                            += world_item_get_price((s16)g_world_formation_unit_pointers[slot]->equipment[i++]);
                    } while (i < 5);
                    if (world_shop_obtain_gil(0) < g_world_shop_service_fee) {
                        world_thread_set_parameters(2, 0x19, 0xF823, 0);
                        main_party_remove_unit(g_world_formation_unit_pointers[slot]->roster_slot);
                        g_world_shop_hire_result_shown = 1;
                    } else {
                        g_world_formation_selected_unit_index = slot;
                        world_formation_stage_selected_unit();
                        g_world_formation_scroll_enabled = 0;
                        g_world_formation_scroll_enabled_latch = 0;
                        world_thread_set_parameters(2, 0x19, 0xF824, 0);
                        g_world_shop_menu_step = 0x15;
                    }
                } else {
                    world_thread_set_parameters(2, 0x19, 0xF822, 0);
                    g_world_shop_hire_result_shown = 1;
                }
            }
        }
    }
    world_shop_run_obtain_gil_menu();
    world_shop_run_soldier_office_fee_menu_script();
    world_menu_run_script_with_callback_suppressed();
}

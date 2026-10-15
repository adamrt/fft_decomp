#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"

/*
 * Refresh formation-screen state after the selected unit may have changed.
 *
 * On a new selection this snapshots the unit's display data, starts the
 * status-banner slide (g_world_formation_banner_slide_step), and records the scroll direction in
 * g_world_status_display_thread_params.redraw_request; it then toggles the per-menu windows and advances the slide.
 * The caller in world_menu_run_remove_item passes two arguments that this body
 * never reads. Compiled at O1 like its world_formation_stage_selected_unit
 * neighbor.
 */
void world_formation_update_unit_selection(void) {
    s32 saved;
    s32 previous;
    s32* flags;

    if (g_unit_selected_index != g_world_formation_selected_unit_index_latch) {
        if (g_world_formation_unit_banner_enabled != 0) {
            saved = g_menu_status_display_thread_params.flags;
            if (g_world_formation_unit_banner_at_bottom_latch != g_world_formation_unit_banner_at_bottom) {
                bcopy(&g_world_unit_pointers[g_world_formation_selected_unit_index_latch]->status,
                    &g_world_comparison_unit_stat_summary, sizeof(menu_unit_status_record_t));
                bcopy(&g_world_unit_pointers[g_world_formation_selected_unit_index_latch]->identity,
                    &g_world_comparison_unit_identity, sizeof(menu_unit_status_identity_t));
                menu_toggle_comparison_status_thread(1);
                g_menu_comparison_display_thread_params.flags = saved;
                g_world_formation_unit_banner_at_bottom_latch = g_world_formation_unit_banner_at_bottom;
                if (g_world_formation_banner_slide_step < 0) {
                    g_world_formation_banner_slide_step += 0x10;
                } else if (g_world_formation_banner_slide_step > 0) {
                    g_world_formation_banner_slide_step -= 0x10;
                } else if (g_world_formation_unit_banner_at_bottom != 0) {
                    g_world_formation_banner_slide_step = -1;
                } else {
                    g_world_formation_banner_slide_step = 1;
                }
            }
        } else {
            if ((g_world_formation_unit_banner_at_bottom_latch = g_world_formation_unit_banner_at_bottom) != 0) {
                g_menu_status_display_thread_params.y = 0x90;
            } else {
                g_menu_status_display_thread_params.y = 0;
            }
        }
        previous = g_world_formation_selected_unit_index_latch;
        world_formation_stage_selected_unit();
        if ((g_menu_input_primary_repeat & PSX_PAD_RIGHT) || (g_menu_input_secondary_repeat & PSX_PAD_R1)) {
            g_menu_status_display_thread_params.redraw_request = 1;
        } else if ((g_menu_input_primary_repeat & PSX_PAD_LEFT) || (g_menu_input_secondary_repeat & PSX_PAD_L1)) {
            g_menu_status_display_thread_params.redraw_request = 2;
        } else {
            g_menu_status_display_thread_params.redraw_request = previous < g_unit_selected_index ? 1 : 2;
        }
    } else if (g_world_formation_unit_banner_at_bottom_latch != g_world_formation_unit_banner_at_bottom
        && g_world_formation_banner_slide_step == 0) {
        g_world_formation_unit_banner_at_bottom_latch = g_world_formation_unit_banner_at_bottom;
        if (g_world_formation_unit_banner_enabled != 0) {
            if (g_world_formation_unit_banner_at_bottom != 0) {
                flags = &g_menu_status_display_thread_params.flags;
                *flags |= MENU_STATUS_PANEL_SLIDE_DOWN;
            } else {
                flags = &g_menu_status_display_thread_params.flags;
                *flags |= MENU_STATUS_PANEL_SLIDE_UP;
            }
        } else if (g_world_formation_unit_banner_at_bottom != 0) {
            g_menu_status_display_thread_params.y = 0x90;
        } else {
            g_menu_status_display_thread_params.y = 0;
        }
    }
    if (g_world_formation_current_menu != WORLD_FORMATION_MENU_ABILITY_LEAVE
        && g_world_formation_current_menu != WORLD_FORMATION_MENU_ITEM_LEAVE
        && g_world_formation_current_menu != WORLD_FORMATION_MENU_JOB_WHEEL_LEAVE) {
        world_menu_start_equipment_and_ability_panel_thread(g_world_formation_unit_browse_enabled);
    }
    if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ITEM
        || g_world_formation_current_menu == WORLD_FORMATION_MENU_ITEM_EQUIP
        || g_world_formation_current_menu == WORLD_FORMATION_MENU_ITEM_BEST_FIT) {
        world_menu_toggle_change_banner_panel_thread(1);
    } else if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ABILITY) {
        world_menu_toggle_ability_panel_thread(1);
    }
    menu_toggle_unit_status_thread(
        g_world_formation_unit_banner_enabled != 0 || g_world_formation_unit_browse_enabled != 0);
    if (g_world_formation_banner_slide_step < 0) {
        g_menu_status_display_thread_params.y = g_world_formation_banner_slide_step * 8 + 0x108;
        g_menu_comparison_display_thread_params.y = g_world_formation_banner_slide_step * 8;
        g_world_formation_banner_slide_step--;
        if (g_world_formation_banner_slide_step < -0xf) {
            g_world_formation_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    } else if (g_world_formation_banner_slide_step > 0) {
        g_menu_status_display_thread_params.y = g_world_formation_banner_slide_step * 8 - 0x78;
        g_menu_comparison_display_thread_params.y = g_world_formation_banner_slide_step * 8 + 0x90;
        g_world_formation_banner_slide_step++;
        if (g_world_formation_banner_slide_step >= 0x10) {
            g_world_formation_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    }
}

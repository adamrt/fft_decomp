#include "fft/data.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"

/*
 * Refresh formation-screen state after the selected unit may have changed.
 *
 * On a new selection this snapshots the unit's display data, starts the
 * status-banner slide (g_world_formation_banner_slide_step), and records the scroll direction in
 * g_world_formation_panel_windows; it then toggles the per-menu windows and advances the slide.
 * The caller in world_menu_run_remove_item passes two arguments that this body
 * never reads. Compiled at O1 like its world_formation_stage_selected_unit
 * neighbour.
 */
void world_formation_update_unit_selection(void) {
    s32 saved;
    s32 previous;
    s32* flags;

    if (g_world_formation_selected_unit_index != g_world_formation_selected_unit_index_latch) {
        if (g_world_formation_unit_banner_enabled != 0) {
            saved = g_world_status_display_thread_params.flags;
            if (g_world_formation_scroll_enabled_latch != g_world_formation_scroll_enabled) {
                bcopy(g_world_formation_unit_pointers[g_world_formation_selected_unit_index_latch],
                    g_world_comparison_unit_stat_summary, 0x22);
                bcopy(g_world_formation_unit_pointers[g_world_formation_selected_unit_index_latch]->name_index,
                    &g_world_comparison_unit_identity, 0xe);
                world_menu_toggle_comparison_banner_thread(1);
                g_world_comparison_display_thread_params.flags = saved;
                g_world_formation_scroll_enabled_latch = g_world_formation_scroll_enabled;
                if (g_world_formation_banner_slide_step < 0) {
                    g_world_formation_banner_slide_step += 0x10;
                } else if (g_world_formation_banner_slide_step > 0) {
                    g_world_formation_banner_slide_step -= 0x10;
                } else if (g_world_formation_scroll_enabled != 0) {
                    g_world_formation_banner_slide_step = -1;
                } else {
                    g_world_formation_banner_slide_step = 1;
                }
            }
        } else {
            if ((g_world_formation_scroll_enabled_latch = g_world_formation_scroll_enabled) != 0) {
                g_world_status_display_thread_params.y = 0x90;
            } else {
                g_world_status_display_thread_params.y = 0;
            }
        }
        previous = g_world_formation_selected_unit_index_latch;
        world_formation_stage_selected_unit();
        if ((g_world_input_primary_repeat & PSX_PAD_RIGHT) || (g_world_input_secondary_repeat & PSX_PAD_R1)) {
            g_world_formation_panel_windows[0].enabled = 1;
        } else if ((g_world_input_primary_repeat & PSX_PAD_LEFT) || (g_world_input_secondary_repeat & PSX_PAD_L1)) {
            g_world_formation_panel_windows[0].enabled = 2;
        } else {
            g_world_formation_panel_windows[0].enabled = previous < g_world_formation_selected_unit_index ? 1 : 2;
        }
    } else if (g_world_formation_scroll_enabled_latch != g_world_formation_scroll_enabled
        && g_world_formation_banner_slide_step == 0) {
        g_world_formation_scroll_enabled_latch = g_world_formation_scroll_enabled;
        if (g_world_formation_unit_banner_enabled != 0) {
            if (g_world_formation_scroll_enabled != 0) {
                flags = &g_world_status_display_thread_params.flags;
                *flags |= 0x20;
            } else {
                flags = &g_world_status_display_thread_params.flags;
                *flags |= 0x40;
            }
        } else if (g_world_formation_scroll_enabled != 0) {
            g_world_status_display_thread_params.y = 0x90;
        } else {
            g_world_status_display_thread_params.y = 0;
        }
    }
    if (g_world_formation_current_menu != 0x13 && g_world_formation_current_menu != 0x11
        && g_world_formation_current_menu != 0xf) {
        world_menu_start_equipment_and_ability_panel_thread(g_world_formation_unit_browse_enabled);
    }
    if (g_world_formation_current_menu == 1 || g_world_formation_current_menu == 6
        || g_world_formation_current_menu == 7) {
        world_menu_toggle_change_banner_panel_thread(1);
    } else if (g_world_formation_current_menu == 2) {
        world_menu_toggle_ability_panel_thread(1);
    }
    world_menu_toggle_unit_status_banner(
        g_world_formation_unit_banner_enabled != 0 || g_world_formation_unit_browse_enabled != 0);
    if (g_world_formation_banner_slide_step < 0) {
        g_world_status_display_thread_params.y = g_world_formation_banner_slide_step * 8 + 0x108;
        g_world_comparison_display_thread_params.y = g_world_formation_banner_slide_step * 8;
        g_world_formation_banner_slide_step--;
        if (g_world_formation_banner_slide_step < -0xf) {
            g_world_formation_banner_slide_step = 0;
            world_menu_stop_unit_status_banner_thread(7);
        }
    } else if (g_world_formation_banner_slide_step > 0) {
        g_world_status_display_thread_params.y = g_world_formation_banner_slide_step * 8 - 0x78;
        g_world_comparison_display_thread_params.y = g_world_formation_banner_slide_step * 8 + 0x90;
        g_world_formation_banner_slide_step++;
        if (g_world_formation_banner_slide_step >= 0x10) {
            g_world_formation_banner_slide_step = 0;
            world_menu_stop_unit_status_banner_thread(7);
        }
    }
}

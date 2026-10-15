#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"

/*
 * Restage the formation unit when the selection changes and step the unit
 * banner slide.
 *
 * On a change, the previous unit's display snapshot is kept for the slide
 * (g_world_comparison_unit_stat_summary/g_world_comparison_unit_identity, the second slot after
 * world_formation_stage_selected_unit's copies) and g_world_status_display_thread_params.redraw_request records the
 * direction: 1 for 0x2000/0x8 input or a forward move, 2 otherwise. g_world_formation_banner_slide_step counts the
 * 16-frame banner slide in either direction, driving the banner y positions in
 * g_world_status_display_thread_params.y/g_world_comparison_display_thread_params.y. Compiled at -O1; the flag update
 * goes through a pointer so each branch materialises the address like the target.
 */
void world_shop_update_unit_selection(void) {
    s32 previous;
    s32* flags;

    if (g_world_formation_selected_unit_index != g_world_formation_selected_unit_index_latch) {
        if (g_world_formation_unit_banner_enabled != 0) {
            if (g_world_formation_unit_banner_at_bottom_latch != g_world_formation_unit_banner_at_bottom) {
                bcopy(g_world_unit_pointers[g_world_formation_selected_unit_index_latch],
                    &g_world_comparison_unit_stat_summary, 0x22);
                bcopy(&g_world_unit_pointers[g_world_formation_selected_unit_index_latch]->identity,
                    &g_world_comparison_unit_identity, sizeof(menu_unit_status_identity_t));
                menu_toggle_comparison_status_thread(1);
                g_world_formation_unit_banner_at_bottom_latch = g_world_formation_unit_banner_at_bottom;
                if (g_world_formation_banner_slide_step < 0) {
                    g_world_formation_banner_slide_step += 16;
                } else if (g_world_formation_banner_slide_step > 0) {
                    g_world_formation_banner_slide_step -= 16;
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
        if (g_world_shop_menu_step >= 6 && g_world_shop_menu_step <= 12) {
            world_formation_recalculate_selected_unit_stats();
        } else {
            world_formation_stage_selected_unit();
        }
        if ((g_menu_input_primary_repeat & PSX_PAD_RIGHT) || (g_menu_input_secondary_repeat & PSX_PAD_R1)) {
            g_menu_status_display_thread_params.redraw_request = 1;
        } else if ((g_menu_input_primary_repeat & PSX_PAD_LEFT) || (g_menu_input_secondary_repeat & PSX_PAD_L1)) {
            g_menu_status_display_thread_params.redraw_request = 2;
        } else {
            g_menu_status_display_thread_params.redraw_request
                = previous < g_world_formation_selected_unit_index ? 1 : 2;
        }
    } else {
        if (g_world_formation_unit_banner_at_bottom_latch != g_world_formation_unit_banner_at_bottom
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
    }
    if (g_world_shop_menu_step >= 20) {
        world_menu_start_equipment_and_ability_panel_thread(g_world_formation_unit_browse_enabled);
    } else {
        world_menu_toggle_change_banner_panel_thread(g_world_formation_unit_browse_enabled);
    }
    menu_toggle_unit_status_thread(
        g_world_formation_unit_banner_enabled != 0 || g_world_formation_unit_browse_enabled != 0);
    if (g_world_formation_banner_slide_step < 0) {
        g_menu_status_display_thread_params.y = g_world_formation_banner_slide_step * 8 + 0x108;
        g_menu_comparison_display_thread_params.y = g_world_formation_banner_slide_step * 8;
        g_world_formation_banner_slide_step--;
        if (g_world_formation_banner_slide_step < -15) {
            g_world_formation_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    } else if (g_world_formation_banner_slide_step > 0) {
        g_menu_status_display_thread_params.y = g_world_formation_banner_slide_step * 8 - 0x78;
        g_menu_comparison_display_thread_params.y = g_world_formation_banner_slide_step * 8 + 0x90;
        g_world_formation_banner_slide_step++;
        if (g_world_formation_banner_slide_step >= 16) {
            g_world_formation_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    }
}

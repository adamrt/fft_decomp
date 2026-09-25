#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"

/*
 * Restage the formation unit when the selection changes and step the unit
 * banner slide.
 *
 * On a change, the previous unit's display snapshot is kept for the slide
 * (g_world_comparison_unit_stat_summary/g_world_comparison_unit_identity, the second slot after
 * world_formation_stage_selected_unit's copies) and g_world_formation_panel_windows records the direction: 1 for
 * 0x2000/0x8 input or a forward move, 2 otherwise. g_world_formation_banner_slide_step counts the 16-frame banner slide
 * in either direction, driving the banner y positions in
 * g_world_status_display_thread_params.y/g_world_comparison_display_thread_params.y. Compiled at -O1; the flag update
 * goes through a pointer so each branch materialises the address like the target.
 */
void world_shop_update_unit_selection(void) {
    s32 previous;
    s32 enable;
    s32* flags;

    if (g_world_formation_selected_unit_index != g_world_formation_selected_unit_index_latch) {
        if (g_world_formation_unit_banner_enabled != 0) {
            if (g_world_formation_scroll_enabled_latch != g_world_formation_scroll_enabled) {
                bcopy(g_world_formation_unit_pointers[g_world_formation_selected_unit_index_latch],
                    g_world_comparison_unit_stat_summary, 0x22);
                bcopy(&g_world_formation_unit_pointers[g_world_formation_selected_unit_index_latch]->name_index,
                    &g_world_comparison_unit_identity, 0xE);
                world_menu_toggle_comparison_banner_thread(1);
                g_world_formation_scroll_enabled_latch = g_world_formation_scroll_enabled;
                if (g_world_formation_banner_slide_step < 0) {
                    g_world_formation_banner_slide_step += 16;
                } else if (g_world_formation_banner_slide_step > 0) {
                    g_world_formation_banner_slide_step -= 16;
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
        if ((u16)(g_world_shop_menu_step - 6) < 7) {
            world_formation_recalculate_selected_unit_stats();
        } else {
            world_formation_stage_selected_unit();
        }
        if ((g_world_input_primary_repeat & PSX_PAD_RIGHT) || (g_world_input_secondary_repeat & PSX_PAD_R1)) {
            g_world_formation_panel_windows[0].enabled = 1;
        } else if ((g_world_input_primary_repeat & PSX_PAD_LEFT) || (g_world_input_secondary_repeat & PSX_PAD_L1)) {
            g_world_formation_panel_windows[0].enabled = 2;
        } else {
            g_world_formation_panel_windows[0].enabled = previous < g_world_formation_selected_unit_index ? 1 : 2;
        }
    } else {
        if (g_world_formation_scroll_enabled_latch != g_world_formation_scroll_enabled
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
    }
    if ((s16)g_world_shop_menu_step >= 20) {
        world_menu_start_equipment_and_ability_panel_thread(g_world_formation_unit_browse_enabled);
    } else {
        world_menu_toggle_change_banner_panel_thread(g_world_formation_unit_browse_enabled);
    }
    enable = 0;
    if (g_world_formation_unit_banner_enabled != 0 || g_world_formation_unit_browse_enabled != 0) {
        enable = 1;
    }
    world_menu_toggle_unit_status_banner(enable);
    if (g_world_formation_banner_slide_step < 0) {
        g_world_status_display_thread_params.y = g_world_formation_banner_slide_step * 8 + 0x108;
        g_world_comparison_display_thread_params.y = g_world_formation_banner_slide_step * 8;
        g_world_formation_banner_slide_step--;
        if (g_world_formation_banner_slide_step < -15) {
            g_world_formation_banner_slide_step = 0;
            world_menu_stop_unit_status_banner_thread(7);
        }
    } else if (g_world_formation_banner_slide_step > 0) {
        g_world_status_display_thread_params.y = g_world_formation_banner_slide_step * 8 - 0x78;
        g_world_comparison_display_thread_params.y = g_world_formation_banner_slide_step * 8 + 0x90;
        g_world_formation_banner_slide_step++;
        if (g_world_formation_banner_slide_step >= 16) {
            g_world_formation_banner_slide_step = 0;
            world_menu_stop_unit_status_banner_thread(7);
        }
    }
}

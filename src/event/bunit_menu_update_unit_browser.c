#include "fft/event_bunit.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

/* The target reaches g_bunit_status_display_flags through an address register (lui/addiu, then
 * 0(reg)) at each access site, which is what GCC emits for a dereferenced
 * constant address; a plain global access folds the address into lui/%lo. */
#define BUNIT_REQUEST_FLAGS (*(s32*)&g_bunit_status_display_flags)

/* Per-frame step of the unit browser.
 *
 * When the selected unit differs from g_bunit_unit_comparison_index it republishes that unit's
 * record, restarts the slide animation, and reports the scroll direction in
 * g_bunit_status_display_redraw_request; otherwise it advances the g_bunit_status_banner_slide_step slide and stops
 * thread 7 once the slide completes. */
void bunit_menu_update_unit_browser(void) {
    s32 index;
    s32 enable;
    s8 slide;
    u8 value;

    if (g_unit_selected_index != g_bunit_unit_comparison_index) {
        if (g_bunit_status_banner_enabled != 0) {
            g_bunit_comparison_display_flags = BUNIT_REQUEST_FLAGS;
            BUNIT_REQUEST_FLAGS = 0;
            if (g_bunit_status_banner_at_bottom_latch != g_bunit_status_banner_at_bottom) {
                bcopy(g_bunit_unit_pointers[g_bunit_unit_comparison_index], &g_bunit_panel_comparison_billboard, 0x22);
                bcopy(&g_bunit_unit_pointers[g_bunit_unit_comparison_index]->name_index,
                    g_bunit_panel_comparison_unit_data, 0xE);
                menu_toggle_comparison_status_thread(1);
                g_bunit_status_banner_at_bottom_latch = g_bunit_status_banner_at_bottom;
                if (g_bunit_status_banner_slide_step < 0) {
                    g_bunit_status_banner_slide_step += 0x10;
                } else if (g_bunit_status_banner_slide_step > 0) {
                    g_bunit_status_banner_slide_step -= 0x10;
                } else if (g_bunit_status_banner_at_bottom != 0) {
                    g_bunit_status_banner_slide_step = -1;
                } else {
                    g_bunit_status_banner_slide_step = 1;
                }
            }
        } else {
            value = g_bunit_status_banner_at_bottom;
            g_bunit_status_banner_at_bottom_latch = value;
            if (value != 0) {
                g_bunit_status_display_offset_y = 0x90;
            } else {
                g_bunit_status_display_offset_y = 0;
            }
        }
        index = g_bunit_unit_comparison_index;
        bunit_unit_copy_selected_data();
        if ((g_menu_input_primary_repeat & PSX_PAD_RIGHT) || (g_menu_input_secondary_repeat & PSX_PAD_R1)) {
            g_bunit_status_display_redraw_request = 1;
        } else if ((g_menu_input_primary_repeat & PSX_PAD_LEFT) || (g_menu_input_secondary_repeat & PSX_PAD_L1)) {
            g_bunit_status_display_redraw_request = 2;
        } else {
            g_bunit_status_display_redraw_request = index < g_unit_selected_index ? 1 : 2;
        }
        return;
    }
    if (g_bunit_status_banner_at_bottom_latch != g_bunit_status_banner_at_bottom
        && g_bunit_status_banner_slide_step == 0) {
        g_bunit_status_banner_at_bottom_latch = g_bunit_status_banner_at_bottom;
        if (g_bunit_status_banner_enabled != 0) {
            if (g_bunit_status_banner_at_bottom != 0) {
                BUNIT_REQUEST_FLAGS |= 0x20;
            } else {
                BUNIT_REQUEST_FLAGS |= 0x40;
            }
        } else if (g_bunit_status_banner_at_bottom != 0) {
            g_bunit_status_display_offset_y = 0x90;
        } else {
            g_bunit_status_display_offset_y = 0;
        }
    }
    bunit_panel_toggle_equipment_and_numeric_editor_threads(g_bunit_unit_browse_enabled);
    enable = 0;
    if (g_bunit_status_banner_enabled != 0 || g_bunit_unit_browse_enabled != 0) {
        enable = 1;
    }
    menu_toggle_unit_status_thread(enable);
    slide = g_bunit_status_banner_slide_step;
    if (slide < 0) {
        g_bunit_status_display_offset_y = slide * 8 + 0x108;
        g_bunit_comparison_display_offset_y = slide * 8;
        if (--g_bunit_status_banner_slide_step < -0xF) {
            g_bunit_status_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    } else if (slide > 0) {
        g_bunit_status_display_offset_y = slide * 8 - 0x78;
        g_bunit_comparison_display_offset_y = slide * 8 + 0x90;
        if (++g_bunit_status_banner_slide_step >= 0x10) {
            g_bunit_status_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    }
}

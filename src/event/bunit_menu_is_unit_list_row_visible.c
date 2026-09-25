#include "fft/event_bunit.h"

/* Decide whether the animated unit-list page has a row available to draw. */
s32 bunit_menu_is_unit_list_row_visible(s32 page_offset) {
    s32 is_visible;
    s8 offset;
    s32 scroll_step;

    offset = page_offset;
    if (g_bunit_unit_browse_enabled == 0) {
        is_visible = 1;
        scroll_step = g_bunit_unit_list_scroll_step;
        if (scroll_step < 0) {
            offset = page_offset - 0x3c;
        }
        if (scroll_step > 0) {
            offset += 0x3c;
        }
        switch (offset) {
        case -0x78:
            if (g_bunit_unit_selected_index >= 0x10) {
                is_visible = 0;
            }
            break;
        case -0x3c:
            if (g_bunit_unit_selected_index >= 0xc) {
                is_visible = 0;
            }
            break;
        case 0:
            if (g_bunit_unit_selected_index >= 8) {
                is_visible = 0;
            }
            break;
        }
    } else {
        is_visible = 0;
    }
    return is_visible;
}

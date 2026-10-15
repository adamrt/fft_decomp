#include "fft/event_bunit.h"

/* Returns 1 when the selected unit is in the two upper visible rows of the list at
 * `page_offset` (after the scroll in progress), which puts the status banner at the bottom
 * of the screen. Returns 0 while browsing units. */
s32 bunit_menu_is_selected_unit_in_upper_rows(s32 page_offset) {
    s32 in_upper_rows;
    s8 offset;
    s32 scroll_step;

    offset = page_offset;
    if (g_bunit_unit_browse_enabled == 0) {
        in_upper_rows = 1;
        scroll_step = g_bunit_unit_list_scroll_step;
        if (scroll_step < 0) {
            offset = page_offset - 0x3c;
        }
        if (scroll_step > 0) {
            offset += 0x3c;
        }
        switch (offset) {
        case -0x78:
            if (g_unit_selected_index >= 0x10) {
                in_upper_rows = 0;
            }
            break;
        case -0x3c:
            if (g_unit_selected_index >= 0xc) {
                in_upper_rows = 0;
            }
            break;
        case 0:
            if (g_unit_selected_index >= 8) {
                in_upper_rows = 0;
            }
            break;
        }
    } else {
        in_upper_rows = 0;
    }
    return in_upper_rows;
}

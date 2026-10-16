/* Whether the unit list is scrolling or has to scroll to show the selected unit. */

#include "fft/unit.h"
#include "fft/unit_list.h"
#include "psx/types.h"

/* Returns 1 while the unit list is scrolling, or when the selected unit lies
 * outside the three rows of four shown at the current page offset, so that
 * the list has to scroll. */
s32 unit_list_is_scroll_needed(void) {
    s32 ret = 0;

    if (g_unit_list_scroll_step == 0) {
        switch (g_unit_list_page_offset) {
        case -0x78:
            if (g_unit_selected_index < 8) {
                ret = 1;
            }
            break;
        case -0x3C:
            if (g_unit_selected_index >= 0x10) {
                ret = 1;
            } else if (g_unit_selected_index < 4) {
                ret = 1;
            }
            break;
        case 0:
            if (g_unit_selected_index >= 0xC) {
                ret = 1;
            }
            break;
        }
    } else {
        ret = 1;
    }
    return ret;
}

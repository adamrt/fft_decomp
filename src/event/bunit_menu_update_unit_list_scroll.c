#include "fft/bunit.h"
#include "psx/types.h"

/* Animate the unit list page offset: when idle, start a +4/-4 per-frame step
 * if the selected unit lies outside the page shown at the current offset
 * (0, -0x3C, -0x78 = pages of four units); once a step reaches +/-0x3C,
 * commit it to the offset and stop. */
void bunit_menu_update_unit_list_scroll(s8* step, s8* offset) {
    s8 cur;
    s8 next;

    cur = *step;
    if (cur == 0) {
        switch (*offset) {
        case -0x78:
            if (g_bunit_unit_selected_index < 8) {
                *step = 4;
            }
            break;
        case -0x3C:
            if (g_bunit_unit_selected_index < 0x10) {
                if (g_bunit_unit_selected_index < 4) {
                    *step = 4;
                }
            } else {
                *step = -4;
            }
            break;
        case 0:
            if (g_bunit_unit_selected_index >= 0xC) {
                *step = -4;
            }
            break;
        }
    } else if (cur < 0) {
        next = cur - 4;
        *step = next;
        if (next < -0x3B) {
            *step = 0;
            *offset -= 0x3C;
        }
    } else if (cur > 0) {
        next = cur + 4;
        *step = next;
        if (next >= 0x3C) {
            *step = 0;
            *offset += 0x3C;
        }
    }
}

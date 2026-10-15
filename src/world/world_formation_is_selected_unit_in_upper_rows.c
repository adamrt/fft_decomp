#include "fft/world.h"
#include "psx/types.h"

/* Returns 1 when the selected unit is in the two upper visible rows of the grid at
 * `scroll_position` (after the scroll in progress), which puts the unit banner at the bottom
 * of the screen. Returns 0 while browsing a unit that is not an egg. */
s32 world_formation_is_selected_unit_in_upper_rows(s32 scroll_position) {
    s32 result = 1;

    if (g_world_formation_unit_browse_enabled == 0
        || (g_world_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG)) {
        if (g_world_formation_scroll_velocity < 0) {
            scroll_position -= 0x3c;
        }
        if (g_world_formation_scroll_velocity > 0) {
            scroll_position += 0x3c;
        }
        switch ((s8)scroll_position) {
        case -0x78:
            if (g_unit_selected_index >= 0x10) {
                result = 0;
            }
            break;
        case -0x3c:
            if (g_unit_selected_index >= 0xc) {
                result = 0;
            }
            break;
        case 0:
            if (g_unit_selected_index >= 8) {
                result = 0;
            }
            break;
        }
    } else {
        result = 0;
    }
    return result;
}

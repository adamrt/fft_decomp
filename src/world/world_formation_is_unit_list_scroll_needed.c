#include "fft/world.h"
#include "psx/types.h"

/* Returns 1 while the unit grid is scrolling, or when the selected unit lies outside the
 * three rows shown at the current page offset, so that the grid has to scroll. */
s32 world_formation_is_unit_list_scroll_needed(void) {
    s32 result = 0;

    if (g_world_formation_scroll_velocity != 0) {
        result = 1;
    } else {
        switch (g_world_formation_scroll_position) {
        case -0x78:
            if (g_unit_selected_index < 8) {
                result = 1;
            }
            break;
        case -0x3C:
            if (g_unit_selected_index >= 0x10) {
                result = 1;
            } else if (g_unit_selected_index < 4) {
                result = 1;
            }
            break;
        case 0:
            if (g_unit_selected_index >= 0xC) {
                result = 1;
            }
            break;
        }
    }
    return result;
}

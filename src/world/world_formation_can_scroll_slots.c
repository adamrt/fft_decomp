#include "fft/data.h"
#include "fft/world.h"
#include "psx/types.h"

s32 world_formation_can_scroll_slots(s32 scroll_position) {
    s32 result = 1;

    if (g_world_formation_unit_browse_enabled == 0
        || (g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->gender_flags & UNIT_FLAG_EGG)) {
        if (g_world_formation_scroll_velocity < 0) {
            scroll_position -= 0x3c;
        }
        if (g_world_formation_scroll_velocity > 0) {
            scroll_position += 0x3c;
        }
        switch ((s8)scroll_position) {
        case -0x78:
            if (g_world_formation_selected_unit_index >= 0x10) {
                result = 0;
            }
            break;
        case -0x3c:
            if (g_world_formation_selected_unit_index >= 0xc) {
                result = 0;
            }
            break;
        case 0:
            if (g_world_formation_selected_unit_index >= 8) {
                result = 0;
            }
            break;
        }
    } else {
        result = 0;
    }
    return result;
}

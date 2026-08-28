#include "fft/world.h"
#include "psx/types.h"

/* Companion of world_formation_can_scroll_slots for the opposite scroll
 * direction: reports whether the selected slot may still move while the
 * roster is parked at one of the three page offsets. */
s32 world_formation_can_scroll_slots_back(void) {
    s32 result = 0;

    if (g_world_formation_scroll_velocity != 0) {
        result = 1;
    } else {
        switch (g_world_formation_scroll_position) {
        case -0x78:
            if (g_world_formation_selected_unit_index < 8) {
                result = 1;
            }
            break;
        case -0x3C:
            if (g_world_formation_selected_unit_index < 0x10) {
                /* The target reloads the index for the inner test: cse
                 * shares it across the dominated block otherwise. */
                __asm__ volatile("" : : : "memory");
                if (g_world_formation_selected_unit_index < 4) {
                    result = 1;
                }
            } else {
                result = 1;
            }
            break;
        case 0:
            if (g_world_formation_selected_unit_index >= 0xC) {
                result = 1;
            }
            break;
        }
    }
    return result;
}

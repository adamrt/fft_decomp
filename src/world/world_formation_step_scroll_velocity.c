#include "fft/world.h"
#include "psx/types.h"

void world_formation_step_scroll_velocity(s8* velocity, s8* position) {
    s8 next;
    s32 value;

    if (*velocity == 0) {
        switch (*position) {
        case -0x78:
            if (g_world_formation_selected_unit_index < 8) {
                *velocity = 4;
            }
            break;
        case -0x3C:
            if (g_world_formation_selected_unit_index >= 0x10) {
                *velocity = -4;
            } else if (g_world_formation_selected_unit_index < 4) {
                *velocity = 4;
            }
            break;
        case 0:
            if (g_world_formation_selected_unit_index >= 0xC) {
                *velocity = -4;
            }
            break;
        }
    } else if (*velocity < 0) {
        next = *velocity - 4;
        *velocity = next;
        if (next < -0x3B) {
            *velocity = 0;
            value = (u8)*position;
            *position = value - 0x3C;
        }
    } else if (*velocity > 0) {
        next = *velocity + 4;
        *velocity = next;
        if (next >= 0x3C) {
            *velocity = 0;
            value = (u8)*position;
            *position = value + 0x3C;
        }
    }
}

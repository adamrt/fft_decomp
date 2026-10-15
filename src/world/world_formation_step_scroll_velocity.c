#include "fft/world.h"
#include "psx/types.h"

/*
 * Steps the formation unit grid's vertical scroll.
 *
 * `position` rests at 0, -0x3C or -0x78 (three of the five rows of four are
 * visible at a time). At rest, a selected unit outside the visible rows
 * starts a scroll; `velocity` then holds the signed pixels covered so far,
 * growing by 4 a call, and after one row height (0x3C) it folds into
 * `position` and returns to 0.
 */
void world_formation_step_scroll_velocity(s8* velocity, s8* position) {
    s8 next;

    if (*velocity == 0) {
        switch (*position) {
        case -0x78:
            if (g_unit_selected_index < 8) {
                *velocity = 4;
            }
            break;
        case -0x3C:
            if (g_unit_selected_index >= 0x10) {
                *velocity = -4;
            } else if (g_unit_selected_index < 4) {
                *velocity = 4;
            }
            break;
        case 0:
            if (g_unit_selected_index >= 0xC) {
                *velocity = -4;
            }
            break;
        }
    } else if (*velocity < 0) {
        next = *velocity - 4;
        *velocity = next;
        if (next < -0x3B) {
            *velocity = 0;
            *position -= 0x3C;
        }
    } else if (*velocity > 0) {
        next = *velocity + 4;
        *velocity = next;
        if (next >= 0x3C) {
            *velocity = 0;
            *position += 0x3C;
        }
    }
}

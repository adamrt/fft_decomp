#include "fft/event_bunit.h"
#include "psx/pad.h"

/* Move a wrapped grid selection in response to directional controller input. */
s32 bunit_menu_update_wrapped_grid_selection(s32 step, s32 count, s32 max, s32 current, s32 input) {
    s32 selection = current;

    if (input & PSX_PAD_UP) {
        selection -= step;
        if (selection < 0) {
            selection += step * count;
            if (max < selection) {
                do {
                    selection -= step;
                } while (max < selection);
            }
        }
    } else if (input & PSX_PAD_DOWN) {
        s32 row = selection / step;
        s32 column = selection % step;
        selection += step;
        if (row == max / step) {
            selection = column;
        } else if (max < selection) {
            selection = max;
        }
    }

    if (input & PSX_PAD_RIGHT) {
        s32 next = selection + 1;
        if (next % step == 0 || max < next) {
            selection = step * (selection / step);
        } else {
            selection = next;
        }
    } else if (input & PSX_PAD_LEFT) {
        s32 next;
        if (selection % step == 0) {
            next = selection + step - 1;
        } else {
            next = selection - 1;
        }
        selection = next;
        if (max < selection) {
            selection = max;
        }
    } else if (input & PSX_PAD_R1) {
        selection++;
        if (max < selection) {
            selection = 0;
        }
    } else if (input & PSX_PAD_L1) {
        selection--;
        if (selection < 0) {
            selection = max;
        }
    }

    if (selection != current) {
        g_bunit_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
    return selection;
}

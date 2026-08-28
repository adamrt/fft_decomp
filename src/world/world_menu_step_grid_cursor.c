#include "fft/main_sound.h"
#include "fft/world.h"
#include "psx/pad.h"

/* Move a grid cursor with row wrapping and partial-last-row clamping.
 *
 * Vertical movement precedes horizontal movement. R1 and L1 move sequentially
 * through the list; changing the selection requests sound 3.
 */
s32 world_menu_step_grid_cursor(s32 columns, s32 rows, s32 last_index, s32 old_index, s32 input) {
    s32 index = old_index;

    if (input & PSX_PAD_UP) {
        index -= columns;
        if (index < 0) {
            index += columns * rows;
            if (last_index < index) {
                do {
                    index -= columns;
                } while (last_index < index);
            }
        }
    } else if (input & PSX_PAD_DOWN) {
        s32 row = index / columns;
        s32 column = index % columns;

        index += columns;
        if (row == last_index / columns) {
            index = column;
        } else if (last_index < index) {
            index = last_index;
        }
    }

    if (input & PSX_PAD_RIGHT) {
        s32 next = index + 1;

        if (next % columns == 0 || last_index < next) {
            index = columns * (index / columns);
        } else {
            index = next;
        }
    } else if (input & PSX_PAD_LEFT) {
        s32 next;

        if (index % columns == 0) {
            next = index + columns - 1;
        } else {
            next = index - 1;
        }
        index = next;
        if (last_index < index) {
            index = last_index;
        }
    } else if (input & PSX_PAD_R1) {
        index++;
        if (last_index < index) {
            index = 0;
        }
    } else if (input & PSX_PAD_L1) {
        index--;
        if (index < 0) {
            index = last_index;
        }
    }

    if (index != old_index) {
        g_world_menu_sound_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
    return index;
}

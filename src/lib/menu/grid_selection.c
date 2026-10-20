/* Moving a grid cursor with row wrapping, clamped on a partial last row. */

#include "fft/menu_selection.h"
#include "fft/sound.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Move a grid cursor with row wrapping and partial-last-row clamping.
 *
 * Vertical movement precedes horizontal movement: Down on the last row returns to the first
 * row's column, and Right at a row's end wraps to its start. R1 and L1 move sequentially
 * through the list; changing the selection queues the cursor-move sound.
 */
s32 menu_grid_selection_step(s32 columns, s32 rows, s32 last_index, s32 old_index, s32 input) {
    s32 index = old_index;

    if (input & PSX_PAD_UP) {
        index -= columns;
        if (index < 0) {
            index += columns * rows;
            while (last_index < index) {
                index -= columns;
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
        g_sound_queued_effect_id = SOUND_SFX_CURSOR_MOVE;
    }
    return index;
}

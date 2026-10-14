#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "psx/pad.h"

/* While the menu may accept input, move a row cursor with Up and Down, wrapping
 * between row 0 and the entry's max_row_index, and play the cursor sound. */
void battle_menu_step_wrapping_cursor_on_scroll_buttons(menu_entry_t* ctx, s32* sel) {
    s32 selection;
    s32 max;
    s32 cur;

    if (menu_can_accept_input() != 0) {
        if (g_script_event_input & PSX_PAD_UP) {
            selection = *sel;
            if (selection == 0) {
                selection = ctx->max_row_index;
            } else {
                selection = selection - 1;
            }
            *sel = selection;
            sound_queue_sfx_cursor_move();
        }
        if (g_script_event_input & PSX_PAD_DOWN) {
            max = ctx->max_row_index;
            cur = *sel;
            if (cur == max) {
                *sel = 0;
            } else {
                *sel = cur + 1;
            }
            sound_queue_sfx_cursor_move();
        }
    }
}

/* Reading the tutorial's scripted buttons or the controller, soft resetting on the reset combination. */

#include "fft/menu/menu_screen.h"
#include "fft/sys/input.h"
#include "fft/sys/memcard.h"
#include "fft/world/save.h"
#include "fft/world/tutorial.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Return the tutorial's scripted 16-bit button mask while a tutorial runs, pad 0
 * otherwise. The soft-reset combination resets the game. */
s32 menu_screen_read_buttons(s32 unused_0) {
    u32 input;

    if (g_tutorial_id != 0) {
        input = g_tutorial_pressed_button;
    } else {
        input = PadRead(0);
    }
    if (input == INPUT_RESET_BUTTONS) {
        memcard_close_stub();
        menu_screen_reset_game();
    }
    return input;
}

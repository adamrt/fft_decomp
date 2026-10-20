/* Reading the tutorial's scripted buttons or the controller, soft resetting on the reset combination. */

#include "fft/input.h"
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/tutorial.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Return the tutorial's scripted 16-bit button mask while a tutorial runs, pad 0
 * otherwise. The soft-reset combination resets the game. */
s32 screen_buttons_read(s32 unused_0) {
    u32 input;

    if (g_tutorial_id != 0) {
        input = g_tutorial_pressed_button;
    } else {
        input = PadRead(0);
    }
    if (input == INPUT_RESET_BUTTONS) {
        save_close_stub();
        screen_game_reset();
    }
    return input;
}

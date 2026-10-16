#include "fft/main.h"
#include "fft/world.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Return the tutorial's scripted 16-bit button mask while a tutorial runs, pad 0
 * otherwise. The soft-reset combination resets the game. */
s32 world_menu_input_read_tutorial_or_controller(s32 unused_0) {
    u32 input;

    if (g_world_script_tutorial_id != 0) {
        input = g_world_script_tutorial_shift_command_bit;
    } else {
        input = PadRead(0);
    }
    if (input == MAIN_INPUT_RESET_BUTTONS) {
        world_card_close_stub();
        world_game_reset();
    }
    return input;
}

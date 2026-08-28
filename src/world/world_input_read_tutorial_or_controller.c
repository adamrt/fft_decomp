#include "fft/main_runtime.h"
#include "fft/world.h"
#include "psx/types.h"

/* L1 + R1 + Select + Start restarts the game. */
#define TUTORIAL_RESET_BUTTONS 0x90C

s32 world_input_read_tutorial_or_controller(void) {
    u32 input;

    if (g_world_script_tutorial_id != 0) {
        input = g_world_script_tutorial_shift_command_bit;
    } else {
        input = PadRead(0);
    }
    if (input == TUTORIAL_RESET_BUTTONS) {
        world_noop_801325d4();
        world_game_reset();
    }
    return input;
}

#include "fft/open.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Read the pad and derive the held and newly pressed buttons, both 0 while
 * OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED is set. L1, R1, Select and Start held
 * with nothing else reset the game. OPEN_SYSTEM_RUNTIME_FLAG_SOFT_RESET_DISABLED
 * would prevent that; no code in OPEN.BIN sets it. */
void open_input_update_buttons_and_check_game_reset(void) {
    u32 input;
    u32 previous;

    g_input_previous_buttons = g_input_polled_buttons;
    input = PadRead(0);
    g_input_polled_buttons = input;

    if (g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_INPUT_DISABLED) {
        g_open_input_new_button_presses = 0;
        g_input_current_buttons = 0;
    } else {
        previous = g_input_previous_buttons;
        g_input_current_buttons = input;
        g_open_input_new_button_presses = input & ~previous;
    }

    if (!(g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_SOFT_RESET_DISABLED)
        && g_input_polled_buttons == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        DrawSync(0);
        suzuki_sound_set_master_volume(0x3fff, 1);
        suzuki_sound_put_type(0xc0);
        open_sound_set_cd_mix_and_volume(0, 1);
        main_system_reset_game();
    }
}

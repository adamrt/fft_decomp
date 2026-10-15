#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* WLDCORE's per-frame pad poll: reads pad 0 and publishes the held buttons
 * and the newly pressed ones (both 0 while MAIN_SYSTEM_FLAG_INPUT_BLOCKED is
 * set). Exactly L1 + R1 + Select + Start restores the sound volumes and
 * resets the game. */
void wldcore_reset_game_if_special_keycode_is_pressed(void) {
    u32 input;
    u32 previous;

    g_input_previous_buttons = g_input_polled_buttons;
    input = PadRead(0);
    g_input_polled_buttons = input;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) {
        g_wldcore_input_new_button_presses = 0;
        g_input_current_buttons = 0;
    } else {
        previous = g_input_previous_buttons;
        g_input_current_buttons = input;
        g_wldcore_input_new_button_presses = input & ~previous;
    }

    if (g_input_polled_buttons == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        DrawSync(0);
        suzuki_sound_set_master_volume(0x3fff, 1);
        suzuki_sound_put_type(0xc0);
        suzuki_sound_set_cd_volume(0x74ff, 1);
        main_system_reset_game();
    }
}

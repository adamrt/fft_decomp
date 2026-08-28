#include "fft/main_sound.h"
#include "fft/open.h"
#include "psx/pad.h"
#include "psx/types.h"

void open_input_update_buttons_and_check_game_reset(void) {
    u32 input;
    u32 previous;

    g_open_input_previous_buttons = g_open_input_polled_buttons;
    input = PadRead(0);
    g_open_input_polled_buttons = input;

    if (g_open_system_runtime_flags & 0x10) {
        g_open_input_new_button_presses = 0;
        g_open_input_current_buttons = 0;
    } else {
        previous = g_open_input_previous_buttons;
        g_open_input_current_buttons = input;
        g_open_input_new_button_presses = input & ~previous;
    }

    if (!(g_open_system_runtime_flags & 0x80000)
        && g_open_input_polled_buttons == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        DrawSync(0);
        main_sound_set_master_volume(0x3fff, 1);
        main_sound_put_type(0xc0);
        open_sound_set_type_and_volume(0, 1);
        main_system_reset_game();
    }
}

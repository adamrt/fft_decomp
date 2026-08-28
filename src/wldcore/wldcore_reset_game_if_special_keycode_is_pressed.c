#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

void wldcore_reset_game_if_special_keycode_is_pressed(void) {
    u32 input;
    u32 previous;

    g_wldcore_previous_button_input = g_wldcore_polled_button_input;
    input = PadRead(0);
    g_wldcore_polled_button_input = input;

    if (g_main_system_flags & 0x4) {
        g_wldcore_new_button_presses = 0;
        g_wldcore_current_button_input = 0;
    } else {
        previous = g_wldcore_previous_button_input;
        g_wldcore_current_button_input = input;
        g_wldcore_new_button_presses = input & ~previous;
    }

    if (g_wldcore_polled_button_input == (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_SELECT | PSX_PAD_START)) {
        DrawSync(0);
        main_sound_set_master_volume(0x3fff, 1);
        main_sound_put_type(0xc0);
        main_sound_set_cd_volume(0x74ff, 1);
        main_system_reset_game();
    }
}

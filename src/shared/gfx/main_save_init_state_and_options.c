#include "fft/battle_gfx.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

/* Clear the 32-bit script/save values and restore the new-game calendar, input
 * timings and packed game options. */
void main_save_init_state_and_options(void) {
    int i;
    u32 value;

    for (i = sizeof(g_main_script_variables) / sizeof(g_main_script_variables[0]) - 1; i >= 0; i--) {
        g_main_script_variables[i] = 0;
    }

    value = 4;
    g_main_input_repeat_period = value;
    g_main_menu_scroll_slow_step = value;
    g_main_menu_scroll_fast_step = value;
    value = g_main_game_options_defaults.value;
    g_main_script_variables[EVENT_SCRIPT_VAR_MONTH] = 1;
    g_main_script_variables[EVENT_SCRIPT_VAR_DAY] = 1;
    g_main_input_repeat_initial_delay = 14;
    g_main_input_secondary_repeat_period = 10;
    g_main_menu_scroll_accel_delay = 32;
    D_800473A7 = 1;
    g_main_common_menu_graphics_loaded = 0;
    g_main_game_options.value = value;
}

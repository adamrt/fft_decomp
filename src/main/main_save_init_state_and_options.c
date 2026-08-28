#include "fft/main_runtime.h"
#include "psx/types.h"

/* The packed-word views of the two options records. g_main_game_options and
 * g_main_game_options_defaults name the same two addresses under their union
 * type; the scalar names are what stop GCC coalescing the load. */
extern u32 g_main_game_options_raw;
extern volatile u32 g_main_game_options_defaults_raw;

/* Clear the script/save words and restore the new-game calendar, input
 * timings and packed game options. */
void main_save_init_state_and_options(void) {
    int remaining = 0xff;
    u32* word = (u32*)&g_main_script_variables[0xff];
    u32 value;

    do {
        *word = 0;
        remaining--;
        word--;
    } while (remaining >= 0);

    value = 4;
    g_main_input_repeat_period = value;
    g_main_menu_scroll_slow_step = value;
    g_main_menu_scroll_fast_step = value;
    value = g_main_game_options_defaults_raw;
    g_main_save_month = 1;
    g_main_save_day = 1;
    g_main_input_repeat_initial_delay = 14;
    g_main_input_secondary_repeat_period = 10;
    g_main_menu_scroll_accel_delay = 32;
    D_800473A7 = 1;
    g_main_common_menu_graphics_loaded = 0;
    g_main_game_options_raw = value;
}

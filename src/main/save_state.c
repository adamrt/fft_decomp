/* Resident script/save variables and their initial calendar, options and input timings. */

#include "fft/input.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/option.h"
#include "fft/save.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/system.h"

/* Resident event variables survive overlay loads and are preserved in save images. */
s32 g_script_variable_storage[0x100];
static const game_options_t s_game_options_defaults = { 0x00201648 };

/* Set with the input repeat timings; no reader has been identified. */
extern u8 g_menu_scroll_unread_setting;

/* Clear the 32-bit script/save values and restore the new-game calendar, input
 * timings and packed game options. */
void save_state_init(void) {
    int i;
    u32 value;

    for (i = sizeof(g_script_variable_storage) / sizeof(g_script_variable_storage[0]) - 1; i >= 0; i--) {
        g_script_variable_storage[i] = 0;
    }

    value = 4;
    g_input_repeat_period = value;
    g_menu_scroll_slow_step = value;
    g_menu_scroll_fast_step = value;
    value = s_game_options_defaults.value;
    g_script_variable_storage[EVENT_SCRIPT_VAR_MONTH] = 1;
    g_script_variable_storage[EVENT_SCRIPT_VAR_DAY] = 1;
    g_input_repeat_initial_delay = 14;
    g_input_secondary_repeat_period = 10;
    g_menu_scroll_accel_delay = 32;
    g_menu_scroll_unread_setting = 1;
    g_menu_common_graphics_loaded = 0;
    g_game_options.value = value;
}

/* Uncalled return-zero wrapper; the resident bank's raw word 0xff gates the
 * BATTLE call. This word is distinct from logical script variable 0xff. */
int overlay_call_battle_return_zero(void) {
    if (g_script_variable_storage[0xff] == 0) {
        return 0;
    }
    return script_return_zero_1();
}

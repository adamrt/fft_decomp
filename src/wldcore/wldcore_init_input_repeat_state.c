#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_init_input_repeat_state(void) {
    g_wldcore_input_repeat_counter_l1 = 0;
    g_wldcore_input_repeat_counter_r1 = 0;
    g_wldcore_input_repeat_counter_right = 0;
    g_wldcore_input_repeat_counter_left = 0;
    g_wldcore_input_repeat_counter_down = 0;
    g_wldcore_input_repeat_counter_up = 0;
    g_wldcore_input_repeat_initial_delay = g_main_input_repeat_initial_delay;
    g_wldcore_input_repeat_period = g_main_input_repeat_period;
    g_wldcore_input_secondary_repeat_period = g_main_input_secondary_repeat_period;
}

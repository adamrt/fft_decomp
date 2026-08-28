#include "fft/main_runtime.h"
#include "fft/open.h"
#include "psx/types.h"

void open_input_init_directional_state(void) {
    s32 first_limit = g_main_input_repeat_initial_delay;
    s32 second_limit = g_main_input_repeat_period;

    g_open_input_direction_counter_3 = 0;
    g_open_input_direction_counter_2 = 0;
    g_open_input_down_repeat_counter = 0;
    g_open_input_up_repeat_counter = 0;
    g_open_input_directional_repeat_initial_delay = first_limit;
    g_open_input_directional_repeat_period = second_limit;
}

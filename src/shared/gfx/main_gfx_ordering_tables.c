#include "fft/battle_gfx.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

u32* main_gfx_get_otag(void) {
    return g_battle_gfx_current_render_buffer->otag;
}

/* Make the other of BATTLE's two render buffers the current one and clear its
 * 0x180-entry ordering table. */
void main_gfx_swap_and_clear_otag(void) {
    battle_render_buffer_t* stats = g_battle_render_buffers;
    u32* otag;

    if (g_battle_gfx_current_render_buffer == stats) {
        stats++;
    }
    otag = stats->otag;
    g_battle_gfx_current_render_buffer = stats;
    ClearOTagR(otag, 0x180);
}

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

int main_overlay_call_battle_return_zero(void) {
    /* Script variable 0xff gates the BATTLE call. */
    if (g_main_script_variables[0xff] == 0) {
        return 0;
    }
    return script_return_zero_1();
}

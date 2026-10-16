/* System report and exception handlers, ordering table access and swap, and save state and options init. */

#include "fft/battle_gfx.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Empty report hook. Callers pass a source number (0 from main_system_handle_error, 1 the effect
 * code, 7 the menu's system functions, 0x11 the dead-unit menu) and a code. */
void main_system_report_stub(s32 category, s32 code) {
}

/* Empty error hook. Callers pass an id of the reporting subsystem and a code;
 * both are dropped. */
void main_system_report_error(s32 allocator_id, s32 error_code) {
}

/* The error hook of the file loads, the heap, the SMD loads and the battle sprite code. */
void main_system_handle_error(int error_code) {
    main_system_report_stub(0, error_code);
    g_main_system_graphics_error = 0;
}

/* Error hook BATTLE calls, mostly when a unit lookup by id fails: the code is
 * dropped (main_system_handle_malloc_exception is empty) and
 * g_main_system_graphics_error is cleared. */
void main_system_handle_pointer_exception(int error_code) {
    main_system_report_error(0, error_code);
    g_main_system_graphics_error = 0;
}

/* Return the ordering table of BATTLE's current render buffer; the pointer is a BATTLE.BIN global,
 * so this only works while BATTLE is loaded. */
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

/* Return 0 either way: with script variable 0xff set it calls BATTLE.BIN's script_return_zero_1,
 * which also returns 0. Nothing calls it. */
int main_overlay_call_battle_return_zero(void) {
    /* Script variable 0xff gates the BATTLE call. */
    if (g_main_script_variables[0xff] == 0) {
        return 0;
    }
    return script_return_zero_1();
}

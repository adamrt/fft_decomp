/* System report and exception handlers, ordering table access and swap, and save state and options init. */

#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/option.h"
#include "fft/render.h"
#include "fft/save.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/system.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Cleared by the two error hooks below; its original purpose and any reader are unknown. */
extern s32 g_system_error_hook_word;
static const game_options_t g_game_options_defaults = { 0x00201648 };

/* Set with the input repeat timings; no reader has been identified. */
extern u8 g_menu_scroll_unread_setting;

/* Empty report hook. Callers pass a source number (0 from system_handle_error, 1 the effect
 * code, 7 the menu's system functions, 0x11 the crystal pickup menu) and a code. */
void system_report_stub(s32 category, s32 code) {
}

/* Empty error hook. Callers pass an id of the reporting subsystem and a code;
 * both are dropped. */
void system_report_error(s32 allocator_id, s32 error_code) {
}

/* The error hook of the file loads, the heap, the SMD loads and the battle sprite code. */
void system_handle_error(int error_code) {
    system_report_stub(0, error_code);
    g_system_error_hook_word = 0;
}

/* Error hook BATTLE calls, mostly when a unit lookup by id fails: the code is
 * dropped (system_report_error is empty) and
 * g_system_error_hook_word is cleared. */
void system_handle_pointer_exception(int error_code) {
    system_report_error(0, error_code);
    g_system_error_hook_word = 0;
}

/* Return the ordering table of BATTLE's current render buffer; the pointer is a BATTLE.BIN global,
 * so this only works while BATTLE is loaded. */
u32* gfx_get_battle_otag(void) {
    return g_render_current_buffer->otag;
}

/* Make the other of BATTLE's two render buffers the current one and clear its
 * 0x180-entry ordering table. */
void gfx_swap_and_clear_battle_otag(void) {
    render_buffer_t* buffer = g_render_buffers;
    u32* otag;

    if (g_render_current_buffer == buffer) {
        buffer++;
    }
    otag = buffer->otag;
    g_render_current_buffer = buffer;
    ClearOTagR(otag, 0x180);
}

/* Clear the 32-bit script/save values and restore the new-game calendar, input
 * timings and packed game options. */
void save_init_state_and_options(void) {
    int i;
    u32 value;

    for (i = sizeof(g_script_variable_storage) / sizeof(g_script_variable_storage[0]) - 1; i >= 0; i--) {
        g_script_variable_storage[i] = 0;
    }

    value = 4;
    g_input_repeat_period = value;
    g_menu_scroll_slow_step = value;
    g_menu_scroll_fast_step = value;
    value = g_game_options_defaults.value;
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

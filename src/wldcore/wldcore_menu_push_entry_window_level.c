#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* Opens the entry window for menu kind 4 and pushes a type-1 menu level over
 * it, clearing system flag 0x04000000 while the level is up. */
void wldcore_menu_push_entry_window_level(void) {
    s32 one;
    u32 flags;
    s32 depth;
    s32* counter;

    g_wldcore_menu_result = -1;
    /* The target also passes a1/a2 window offsets to this one-argument callee. */
    ((void (*)(s32, s32, s32))world_menu_open_entry_window)(4, -0x30, -0x38);
    one = 1;
    g_wldcore_context_value_display_mode = one;
    g_wldcore_displayed_numeric_value = world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    depth = g_wldcore_menu_stack_depth;
    flags = g_main_system_flags;
    g_wldcore_menu_stack_records_next[depth].return_level.delay = 3;
    g_wldcore_menu_stack_records_next[depth].return_level.pending = 0;
    g_main_system_flags = flags & 0xFBFFFFFF;
    if ((flags & 8) != 0) {
        g_main_system_flags = (flags & 0xFBFFFFFF) | 0x08000000;
        counter = &g_wldcore_screen_fade_state.elapsed;
        *counter = *counter - 4;
    }
    {
        s32 last;
        last = g_wldcore_menu_stack_depth;
        g_wldcore_menu_stack_types[last + 1] = one;
        g_wldcore_menu_stack_depth = last + 1;
    }
}

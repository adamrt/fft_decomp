#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/open.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Open the entry window and push its world-map return level. */
void wldcore_menu_push_entry_window_level(void) {
    u32 flags;
    s32 depth;
    s32* counter;

    g_menu_blocked_buttons = -1;
    world_menu_open_entry_window(4, -0x30, -0x38);
    g_wldcore_context_value_display_mode = 1;
    g_wldcore_displayed_numeric_value = script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    depth = g_wldcore_menu_stack_depth;
    flags = g_main_system_flags;
    g_wldcore_menu_stack_records_next[depth].return_level.delay = 3;
    g_wldcore_menu_stack_records_next[depth].return_level.pending = 0;
    g_main_system_flags = flags & ~MAIN_SYSTEM_FLAG_SCREEN_CLEARED;
    if ((flags & MAIN_SYSTEM_FLAG_FADING) != 0) {
        g_main_system_flags = (flags & ~MAIN_SYSTEM_FLAG_SCREEN_CLEARED) | MAIN_SYSTEM_FLAG_FREEZE_ANIMATION;
        counter = &g_wldcore_screen_fade_state.elapsed;
        *counter = *counter - 4;
    }
    {
        s32 last;
        last = g_wldcore_menu_stack_depth;
        g_wldcore_menu_stack_types[last + 1] = WLDCORE_MENU_LEVEL_RETURN;
        g_wldcore_menu_stack_depth = last + 1;
    }
}

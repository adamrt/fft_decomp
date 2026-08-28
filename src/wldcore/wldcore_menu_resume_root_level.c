#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

void wldcore_menu_resume_root_level(s32* param) {
    s32 index = param[0];

    g_wldcore_context_value_display_mode = 1;
    g_wldcore_window_records[index].palette = 0;
    g_wldcore_displayed_numeric_value = world_script_get_variable(EVENT_SCRIPT_VAR_WAR_FUNDS);
    if (g_wldcore_window_records[g_wldcore_map_window_index].flags & 0x10) {
        wldcore_gfx_toggle_captured_world_frame(0);
    }
    wldcore_menu_dispatch_pending_script_requests();
}

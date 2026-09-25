#include "fft/wldcore.h"

/* Per-frame handler of the world-map return level: waits out its own delay,
 * then pops the level and dispatches whichever pending system-flag request
 * asked for the return. */
void wldcore_menu_step_return_level(wldcore_menu_return_level_t* level) {
    s32 flags;
    s32 cleared;
    s32 location;

    if (g_main_system_flags & 0x4000000) {
        level->pending = 1;
    }
    if (g_main_system_flags & 8) {
        if (level->pending != 0) {
            level->pending = 0;
            wldcore_gfx_toggle_captured_world_frame(0);
            level->delay = 3;
            g_main_system_flags |= 0x8000008;
        }
        g_wldcore_menu_result = -1;
        return;
    }
    if (level->delay != 0) {
        level->delay -= 1;
        if (level->delay == 0) {
            wldcore_gfx_toggle_captured_world_frame(1);
        }
        return;
    }
    g_wldcore_menu_result = 0;
    if (world_menu_get_thread_activity() != 0) {
        return;
    }
    wldcore_init_input_repeat_state();
    flags = g_main_system_flags;
    wldcore_gfx_toggle_captured_world_frame(0);
    cleared = g_main_system_flags & ~0x8000000;
    g_main_system_flags = cleared;
    g_wldcore_menu_stack_depth -= 1;
    if (flags & 0x100) {
        g_main_system_flags = cleared ^ 2;
        wldcore_menu_push_screen_transition_level(0x1C, 0, 0);
        return;
    }
    if (flags & 0x4000) {
        location = world_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);
        wldcore_init_core();
        wldcore_menu_dispatch_resume_handler();
        if (g_wldcore_map_projection_state.marker.kind != location) {
            wldcore_location_process_entry(g_wldcore_map_projection_state.marker.kind, location);
        }
        return;
    }
    if (flags & 0x100000) {
        g_main_system_flags = cleared ^ 2;
        wldcore_menu_push_screen_transition_level(0x2B, 0, 1);
        return;
    }
    if (flags & 0x800000) {
        wldcore_menu_push_variable_list_level();
        return;
    }
    if (flags & 0x1000000) {
        wldcore_menu_push_fixed_number_panel_level();
        return;
    }
    if (flags & 0x10000) {
        g_main_system_flags = cleared & 0xFFBFFFFE;
        if (world_script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0
            || world_script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) == 0) {
            g_main_system_flags |= 0x10000;
        } else {
            g_main_system_flags &= 0xFFFEFFFF;
        }
        g_main_system_flags |= 0x200;
        return;
    }
    wldcore_menu_dispatch_resume_handler();
}

#include "fft/script.h"
#include "fft/wldcore.h"

/* Per-frame handler of the world-map return level: waits out its own delay,
 * then pops the level and dispatches whichever pending system-flag request
 * asked for the return. */
void wldcore_menu_step_return_level(wldcore_menu_return_level_t* level) {
    s32 flags;
    s32 cleared;
    s32 location;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_SCREEN_CLEARED) {
        level->pending = 1;
    }
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_FADING) {
        if (level->pending != 0) {
            level->pending = 0;
            wldcore_gfx_toggle_captured_world_frame(0);
            level->delay = 3;
            g_main_system_flags |= MAIN_SYSTEM_FLAG_FADING | MAIN_SYSTEM_FLAG_FREEZE_ANIMATION;
        }
        g_menu_blocked_buttons = -1;
        return;
    }
    if (level->delay != 0) {
        level->delay -= 1;
        if (level->delay == 0) {
            wldcore_gfx_toggle_captured_world_frame(1);
        }
        return;
    }
    g_menu_blocked_buttons = 0;
    if (menu_get_thread_activity_variant() != 0) {
        return;
    }
    wldcore_input_init_repeat_state();
    flags = g_main_system_flags;
    wldcore_gfx_toggle_captured_world_frame(0);
    cleared = g_main_system_flags & ~MAIN_SYSTEM_FLAG_FREEZE_ANIMATION;
    g_main_system_flags = cleared;
    g_wldcore_menu_stack_depth -= 1;
    if (flags & MAIN_SYSTEM_FLAG_REQUEST_BRAVE_STORY) {
        g_main_system_flags = cleared ^ MAIN_SYSTEM_FLAG_DRAW_MAP;
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_BRAVE_STORY, 0, 0);
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_REQUEST_DESTINATION) {
        location = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);
        wldcore_init_core();
        wldcore_menu_dispatch_resume_handler();
        if (g_wldcore_map_projection_state.marker.kind != location) {
            wldcore_location_process_entry(g_wldcore_map_projection_state.marker.kind, location);
        }
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_REQUEST_TUTORIAL) {
        g_main_system_flags = cleared ^ MAIN_SYSTEM_FLAG_DRAW_MAP;
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES, 0, 1);
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_REQUEST_VARIABLE_LIST) {
        wldcore_menu_push_variable_list_level();
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL) {
        wldcore_menu_push_fixed_number_panel_level();
        return;
    }
    if (flags & MAIN_SYSTEM_FLAG_RESTART) {
        g_main_system_flags = cleared & ~(MAIN_SYSTEM_FLAG_RUNNING | MAIN_SYSTEM_FLAG_UNLOCK_ALL);
        if (script_get_variable(EVENT_SCRIPT_VAR_FORMATION_RETURN_EVENT) != 0
            || script_get_variable(EVENT_SCRIPT_VAR_NEXT_SCENARIO) == 0) {
            g_main_system_flags |= MAIN_SYSTEM_FLAG_RESTART;
        } else {
            g_main_system_flags &= ~MAIN_SYSTEM_FLAG_RESTART;
        }
        g_main_system_flags |= MAIN_SYSTEM_FLAG_NO_BACKGROUND_IMAGE;
        return;
    }
    wldcore_menu_dispatch_resume_handler();
}

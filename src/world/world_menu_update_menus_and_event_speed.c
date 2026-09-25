#include "fft/battle_ai.h"
#include "fft/battle_state.h"
#include "fft/event.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Per-frame menu/world step: snapshots g_battle_ai_workspace, substitutes the unfiltered
 * controller input while menu input is disabled, clears the four g_world_menu_pending_selection
 * slots to -2, runs the input/update steps and, while threads 6/7 or thread 8
 * (outside state 0x42) are running, forces event speed 2 (restoring the saved
 * animation speed afterwards). Returns world_menu_resolve_selection's result. */
s32 world_menu_update_menus_and_event_speed(u32* ot, u32 input) {
    s32 i;
    s16 value;
    s16* unused;
    s32 result;

    g_battle_ai_workspace_ptr = g_battle_ai_workspace;
    if (g_world_menu_input_disabled != 0) {
        input = g_world_input_unfiltered_controller;
    }
    world_map_store_selected_tile_data();
    value = -2;
    for (i = 3; i >= 0; i--) {
        g_world_menu_pending_selection[i] = value;
    }
    world_script_run_frame(ot, input);
    world_menu_update_panel_fade();
    result = world_menu_resolve_selection();
    if ((world_thread_is_running_80100164(8) != 0
            && g_world_thread_contexts[8].task_id != NATIVE_THREAD_TASK_MENU_SOUND_DELAY)
        || world_thread_is_running_80100164(7) != 0 || world_thread_is_running_80100164(6) != 0) {
        if (g_world_animation_speed_forced == 0) {
            g_world_animation_speed_forced = 1;
            g_world_saved_animation_speed = battle_state_get_animation_speed();
            world_script_set_event_speed(2);
        }
    } else if (g_world_animation_speed_forced == 1) {
        g_world_animation_speed_forced = 0;
        world_script_set_event_speed(g_world_saved_animation_speed);
    }
    return result;
}

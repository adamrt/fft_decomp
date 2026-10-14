#include "fft/battle.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Per-frame menu/world step: snapshots g_ai_workspace, substitutes the unfiltered
 * controller input while menu input is disabled, clears the four g_menu_pending_selection
 * slots to -2, runs the input/update steps and, while threads 6/7 or thread 8
 * (outside state 0x42) are running, forces event speed 2 (restoring the saved
 * animation speed afterwards). Returns world_menu_resolve_selection's result. */
s32 world_menu_update_menus_and_event_speed(u32* ot, u32 input) {
    s32 i;
    s16 value;
    s32 result;

    g_ai_workspace_ptr = (event_work_t*)g_ai_workspace;
    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        input = g_script_unfiltered_controller_input;
    }
    map_store_selected_tile_data();
    value = -2;
    for (i = 3; i >= 0; i--) {
        g_menu_pending_selection[i] = value;
    }
    script_run_event_frame(ot, input);
    menu_update_panel_fade();
    result = world_menu_resolve_selection();
    if ((world_thread_is_running_asm(THREAD_SLOT_MENU) != 0
            && g_thread_contexts[THREAD_SLOT_MENU].task_id != THREAD_TASK_MINI_MENU)
        || world_thread_is_running_asm(THREAD_SLOT_MENU_CHILD) != 0
        || world_thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0) {
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

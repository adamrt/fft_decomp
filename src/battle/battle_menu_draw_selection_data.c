#include "fft/battle.h"
#include "psx/types.h"

/*
 * Run one frame of the battle menu/event layer and return battle_menu_resolve_selection's
 * result. While event threads 6-8 run (thread 8 only when g_battle_thread_contexts[8].task_id is not
 * 0x42) the event speed is forced to 2, restoring the saved speed afterwards.
 *
 * The `fill` staging variable places the -2 load ahead of the loop counter
 * the way the target does; a literal store hoists it after the counter.
 */
s32 battle_menu_draw_selection_data(s32 otag, s32 controller_input) {
    s32 i;
    s32 result;
    s32 fill;

    g_battle_ai_workspace_ptr = (void*)g_battle_ai_workspace;
    if (g_battle_menu_input_disabled != 0) {
        controller_input = g_battle_script_unfiltered_controller_input;
    }
    battle_map_store_selected_tile_data();
    fill = -2;
    for (i = 3; i >= 0; i--) {
        g_battle_menu_pending_selection[i] = fill;
    }
    battle_script_run_event_frame((u32*)otag, controller_input);
    battle_menu_update_panel_fade();
    result = battle_menu_resolve_selection();
    if ((battle_thread_is_running_8014cc94(8) != 0
            && g_battle_thread_contexts[8].task_id != NATIVE_THREAD_TASK_MENU_SOUND_DELAY)
        || battle_thread_is_running_8014cc94(7) != 0 || battle_thread_is_running_8014cc94(6) != 0) {
        if (g_battle_animation_speed_forced == 0) {
            g_battle_animation_speed_forced = 1;
            g_battle_saved_animation_speed = battle_state_get_animation_speed();
            battle_script_set_event_speed(2);
        }
    } else if (g_battle_animation_speed_forced == 1) {
        g_battle_animation_speed_forced = 0;
        battle_script_set_event_speed(g_battle_saved_animation_speed);
    }
    return result;
}

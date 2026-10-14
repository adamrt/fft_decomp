#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Run one frame of the battle menu/event layer and return battle_menu_resolve_selection's
 * result. While event threads 6-8 run (thread 8 only when g_thread_contexts[8].task_id is not
 * 0x42) the event speed is forced to 2, restoring the saved speed afterwards.
 *
 * The `fill` staging variable places the -2 load ahead of the loop counter
 * the way the target does; a literal store hoists it after the counter.
 */
s32 battle_menu_update_menus_and_event_speed(u32* otag, s32 controller_input) {
    s32 i;
    s32 result;
    s32 fill;

    g_ai_workspace_ptr = (event_work_t*)g_ai_workspace;
    if (g_menu_input_mode != MENU_INPUT_PLAYER) {
        controller_input = g_script_unfiltered_controller_input;
    }
    map_store_selected_tile_data();
    fill = -2;
    for (i = 3; i >= 0; i--) {
        g_menu_pending_selection[i] = fill;
    }
    script_run_event_frame(otag, controller_input);
    menu_update_panel_fade();
    result = battle_menu_resolve_selection();
    if ((battle_thread_is_running_asm(THREAD_SLOT_MENU) != 0
            && g_thread_contexts[THREAD_SLOT_MENU].task_id != THREAD_TASK_MINI_MENU)
        || battle_thread_is_running_asm(THREAD_SLOT_MENU_CHILD) != 0
        || battle_thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0) {
        if (g_battle_animation_speed_forced == 0) {
            g_battle_animation_speed_forced = 1;
            g_battle_saved_animation_speed = battle_state_get_animation_speed();
            script_set_event_speed(2);
        }
    } else if (g_battle_animation_speed_forced == 1) {
        g_battle_animation_speed_forced = 0;
        script_set_event_speed(g_battle_saved_animation_speed);
    }
    return result;
}

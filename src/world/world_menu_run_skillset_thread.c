#include "fft/world.h"
#include "psx/types.h"

enum { WORLD_UNKNOWN_ROW_BYTES = 17 };

/* The completion value written to the thread's
 * second parameter is materialised before the branch, which puts `li v1,1`
 * in the beqz delay slot ahead of the 0xFC argument. */
void world_menu_run_skillset_thread(void) {
    s32 param_value;
    u8* row;
    s32* param;

    world_menu_check_action_restrictions();
    g_world_thread_inner_subroutine_callback = world_menu_build_skillset_entries;
    world_thread_call_on_main_stack();
    param_value = 1;
    if (g_world_menu_monster_skillset_flag != 0) {
        g_world_menu_thread_menu_data[4].selected_index = 0;
        if (g_world_menu_restore_pending == 0) {
            world_thread_start(g_world_thread_current_id - 1, world_menu_select_unit_action_slots_thread);
            world_thread_set_parameters(g_world_thread_current_id - 1, (s32)&g_world_menu_thread_menu_data[3], 0, 0);
        } else {
            world_thread_wait_frames(2);
        }
        for (;;) {
            world_thread_yield();
            if (world_thread_is_running_80100164(g_world_thread_current_id - 1) == 0) {
                world_thread_exit_current();
            }
        }
    }
    param = &g_world_thread_contexts[g_world_thread_current_id].function_parameter_2;
    *param = param_value;
    row = g_world_menu_unit_selection_rows[g_world_unit_view_battle_id].bytes;
    g_world_menu_thread_menu_data[4].selected_index = row[2];
    g_world_menu_restore_depth = 1;
    /* The target also passes the menu data in a1 to this one-argument callee. */
    ((void (*)(s32, world_menu_entry_t*))world_script_pulse_tutorial_wait_value)(0xFC, g_world_menu_thread_menu_data);
    world_menu_run_icon_selection_loop();
    row[2] = g_world_menu_thread_menu_data[4].selected_index;
    world_thread_exit_current();
}

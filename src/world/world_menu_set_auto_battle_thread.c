#include "fft/battle.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Applies the auto-battle choice of the menu selection (3 or 4) to the stored unit and confirms the
 * thread's menu entry without a sound. */
void world_menu_set_auto_battle_thread(void) {
    s32 entry = world_thread_get_current_parameter_1();
    battle_stats_t* unit = world_unit_get_battle_stats_for_stored();

    if (((world_menu_thread_data_t*)g_world_menu_thread_menu_data)->selection == 3) {
        unit->auto_battle_setting = 0x10;
    } else if (((world_menu_thread_data_t*)g_world_menu_thread_menu_data)->selection == 4) {
        unit->auto_battle_setting = 0x11;
    }
    g_world_menu_set_auto_battle_row_actions = -3;
    world_menu_confirm_action_silently(entry);
    world_thread_exit_current();
}

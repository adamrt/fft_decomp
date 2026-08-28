#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

extern s32 g_wldcore_next_map_id[];
/* Advance the system flags, reset the script variables and stop this thread. */
void world_script_reset_event_variables_thread(void) {
    g_main_system_flags += 3;
    world_script_set_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE, 1);
    world_script_set_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_MAP_ARRANGEMENT, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_WEATHER_AND_TIME, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_WEATHER, 0);
    world_script_set_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY, 0);
    g_wldcore_next_map_id[0] = g_world_menu_number_entry_value;
    world_thread_exit_current();
}

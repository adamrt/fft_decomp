#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "psx/types.h"

s32 battle_map_check_deep_dungeon_crystal_and_init_state(void) {
    s32 crystal_count;
    s32 previous_count;

    if (g_main_system_frontend_world_result == 4) {
        if (g_battle_map_deep_dungeon_load_in_progress == 0) {
            crystal_count = battle_unit_count_crystal_misc_units();
            if (crystal_count >= 5) {
                crystal_count = 4;
            }
            previous_count = battle_script_get_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT);
            if (crystal_count != previous_count) {
                battle_script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, crystal_count);
                if (battle_map_load_data_stage_0x75() != 0) {
                    g_battle_map_deep_dungeon_load_in_progress = 1;
                } else {
                    battle_script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, previous_count);
                }
            }
        }
        return g_battle_map_deep_dungeon_load_in_progress;
    }
    return 0;
}

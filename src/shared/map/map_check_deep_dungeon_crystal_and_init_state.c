#include "fft/battle_map.h"
#include "fft/script.h"
#include "psx/types.h"

/* Reload the map when the number of crystals on the field has changed.
 *
 * Only in a battle entered with world result 4, and not while such a reload is
 * running: the crystal count, capped at 4, is compared with script variable
 * EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, the map state that GNS rows of
 * those maps select on. A new count is stored and a map data load started; the
 * old count is put back when the load cannot start. Returns whether a reload is
 * in progress. */
s32 map_check_deep_dungeon_crystal_and_init_state(void) {
    s32 crystal_count;
    s32 previous_count;

    if (g_main_system_frontend_world_result == 4) {
        if (g_map_deep_dungeon_load_in_progress == 0) {
            crystal_count = battle_unit_count_crystals();
            if (crystal_count >= 5) {
                crystal_count = 4;
            }
            previous_count = script_get_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT);
            if (crystal_count != previous_count) {
                script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, crystal_count);
                if (map_start_data_load() != 0) {
                    g_map_deep_dungeon_load_in_progress = 1;
                } else {
                    script_set_variable(EVENT_SCRIPT_VAR_DEEP_DUNGEON_CRYSTAL_COUNT, previous_count);
                }
            }
        }
        return g_map_deep_dungeon_load_in_progress;
    }
    return 0;
}

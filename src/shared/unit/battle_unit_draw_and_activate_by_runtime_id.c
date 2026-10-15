#include "fft/battle_unit.h"
#include "psx/types.h"

/* Makes a unit visible: enables its drawing, triggers its sprite graphic and,
 * when it has battle stats, runs battle_unit_set_enemy_level_data_by_battle_id.
 * Returns 0 when no record has the runtime id. */
s32 battle_unit_draw_and_activate_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit;
    unit_stats_t* stats;

    unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);
    if (unit != 0) {
        stats = unit->stats;
        unit->draw_enabled = 1;
        unit->animation.trigger = 1;
        if (stats != 0) {
            battle_unit_set_enemy_level_data_by_battle_id(stats->battle_id);
        }
        return 1;
    }
    return 0;
}

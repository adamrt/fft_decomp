#include "fft/battle_unit.h"

/* The battle unit slot of a runtime record, or -1 when the record has no battle
 * stats. A runtime id without a record raises pointer exception 12 and also returns
 * -1. */
s32 battle_unit_get_battle_id_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit_stats_t* stats = unit->stats;

        if (stats != 0) {
            return stats->battle_id;
        }
    } else {
        main_system_handle_pointer_exception(12);
    }
    return -1;
}

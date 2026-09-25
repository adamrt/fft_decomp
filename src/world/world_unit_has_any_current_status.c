#include "fft/battle.h"
#include "fft/world.h"
#include "psx/types.h"

/* Returns 1 when the unit has any current status byte set. */
s32 world_unit_has_any_current_status(s32 unit_id) {
    battle_stats_t* stats = battle_unit_get_stats_from_battle_id(unit_id);
    s32 i;

    for (i = 0; i < BATTLE_STATUS_BYTE_COUNT; i++) {
        if (stats->status_sets.current[i] != 0) {
            return 1;
        }
    }
    return 0;
}

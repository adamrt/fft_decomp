#include "fft/battle_unit.h"

/* Initialize one battle slot from the current party entry.
 *
 * Only unit_index is observed; no direct caller or callback table is proven. */
s32 battle_unit_init_party_member_battle_data(s32 unused0, s32 unused1, s32 unit_index) {
    unit_stats_t* unit;
    s32 formation_id;

    unit = &g_battle_unit_stats[unit_index];
    formation_id = g_main_unit_deployed_coords->party_id;
    unit->battle_id = unit_index;
    if (main_unit_init_job_data(unit, formation_id, 0) != 0) {
        return -1;
    }
    main_unit_init_status_and_rewards(unit, 1);
    return 0;
}

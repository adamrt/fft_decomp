#include "fft/main_unit.h"

void main_status_store_current(battle_stats_t* unit) {
    battle_stats_t* unit_data = unit;
    s32 status_set;

    for (status_set = 0; status_set < BATTLE_STATUS_BYTE_COUNT; status_set++) {
        unit_data->status_sets.current[status_set]
            = unit_data->status_sets.innate[status_set] | unit_data->inflicted_status[status_set];
    }
}

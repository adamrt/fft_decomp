#include "fft/main.h"

/* Recompute the current status set as innate | inflicted. */
void main_status_store_current(unit_stats_t* unit) {
    s32 status_set;

    for (status_set = 0; status_set < STATUS_BYTE_COUNT; status_set++) {
        unit->status_sets.current[status_set]
            = unit->status_sets.innate[status_set] | unit->inflicted_status[status_set];
    }
}

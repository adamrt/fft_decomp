#include "fft/main.h"

/* Seed inflicted_status with the current statuses the unit does not have
 * innately, then set up the status CTs (main_status_init_ct) and reconcile the
 * status flags. */
void main_status_init_unit(unit_stats_t* unit) {
    s32 status_set;

    for (status_set = 0; status_set < STATUS_BYTE_COUNT; status_set++) {
        unit->inflicted_status[status_set]
            = unit->status_sets.current[status_set] & ~unit->status_sets.innate[status_set];
    }
    main_status_init_ct(unit);
    main_status_update_unit_flags_and_ct(unit);
}

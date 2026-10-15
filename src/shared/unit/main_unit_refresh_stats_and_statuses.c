#include "fft/main.h"

/* main_unit_update_stats_statuses_and_equipment with both flags clear: stats are
 * re-derived and the status notifications run. */
void main_unit_refresh_stats_and_statuses(unit_stats_t* unit) {
    main_unit_update_stats_statuses_and_equipment(unit, 0, 0);
}

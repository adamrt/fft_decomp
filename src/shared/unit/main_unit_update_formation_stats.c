#include "fft/main.h"

/* main_unit_update_stats_statuses_and_equipment with skip_status_check set: stats
 * are re-derived without the critical and status notifications. */
void main_unit_update_formation_stats(unit_stats_t* unit) {
    main_unit_update_stats_statuses_and_equipment(unit, 1, 0);
}

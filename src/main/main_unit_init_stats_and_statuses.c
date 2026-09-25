#include "fft/main.h"
#include "psx/types.h"

void main_unit_init_stats_and_statuses(battle_stats_t* unit) {
    main_unit_update_stats_statuses_and_equipment(unit, 0, 1);
}

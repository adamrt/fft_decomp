#include "fft/main.h"
#include "psx/types.h"

void main_unit_update_formation_stats(battle_stats_t* unit) {
    main_unit_update_stats_statuses_and_equipment(unit, 1, 0);
}

#include "fft/battle.h"
#include "psx/types.h"

void battle_status_disable_acting(battle_stats_t* unit) {
    update_unit_action_statuses(unit, 0);
}

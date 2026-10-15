#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_status.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

/* Clears the unit's Charging, Jump, Defending and Performing statuses (action state 0). */
void status_disable_acting(unit_stats_t* unit) {
    main_status_set_action_state(unit, 0);
}

#include "fft/battle_status.h"
#include "fft/main.h"

/* Clears the unit's Charging, Jump, Defending and Performing statuses (action state 0). */
void status_disable_acting(unit_stats_t* unit) {
    main_status_set_action_state(unit, 0);
}

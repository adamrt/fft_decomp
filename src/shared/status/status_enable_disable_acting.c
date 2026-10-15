#include "fft/battle_status.h"
#include "fft/main.h"

/* Clears the unit's Charging, Jump, Defending and Performing statuses (action
 * state 0), exactly as status_disable_acting does. */
void status_enable_disable_acting(unit_stats_t* unit) {
    main_status_set_action_state(unit, 0);
}

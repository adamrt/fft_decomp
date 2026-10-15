#include "fft/main.h"

/* Charge time of the unit's last ability: the low seven bits of the ability
 * record's CT byte. */
void main_unit_copy_last_ability_ct(unit_stats_t* unit) {
    unit->ability_ct = g_main_ability_range_data[unit->action_command.ability_id].ct & 0x7f;
}

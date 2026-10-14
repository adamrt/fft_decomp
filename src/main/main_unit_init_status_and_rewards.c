#include "fft/main.h"

/* Raise a Speed of 0 to 1, optionally clear the war trophy and bonus money, and
 * initialize the unit's status data (main_status_init_unit). */
void main_unit_init_status_and_rewards(unit_stats_t* unit, s32 clear_rewards) {
    if (unit->attributes[UNIT_ATTRIBUTE_SPEED] == 0) {
        unit->attributes[UNIT_ATTRIBUTE_SPEED] = 1;
    }
    if (clear_rewards != 0) {
        unit->war_trophy = ITEM_ID_NOTHING;
        unit->bonus_money_modifier = 0;
    }
    main_status_init_unit(unit);
}

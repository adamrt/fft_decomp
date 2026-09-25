#include "fft/main.h"

void main_unit_init_status_and_rewards(battle_stats_t* unit, s32 clear_rewards) {
    if (unit->attributes[UNIT_ATTRIBUTE_SPEED] == 0) {
        unit->attributes[UNIT_ATTRIBUTE_SPEED] = 1;
    }
    if (clear_rewards != 0) {
        unit->war_trophy = ITEM_ID_NOTHING;
        unit->bonus_money_modifier = 0;
    }
    main_status_init_unit(unit);
}

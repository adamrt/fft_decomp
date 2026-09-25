#include "fft/main.h"

void main_unit_init_position_and_rewards(battle_stats_t* unit, entd_unit_t* entd) {
    unit->x = entd->x;
    unit->position.bits.y = entd->position.bits.y;
    unit->position.bits.higher_elevation = entd->position.bits.higher_elevation;
    unit->position.bits.facing = entd->position.bits.facing;
    unit->position.bits.spell_quote_skillsets = entd->position.bits.spell_quote_skillsets;

    unit->war_trophy = entd->war_trophy;
    unit->bonus_money_modifier = entd->bonus_money_modifier;
}

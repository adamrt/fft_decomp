#include "fft/battle.h"

void battle_effect_build_secondary_init(battle_unit_misc_data_t* unit, battle_effect_secondary_init_t* effect) {
    effect->caster.fields.target_type = 0;
    effect->caster.fields.caster_id = unit->unit_id;
    effect->target_count = 1;
    effect->target.fields.target_type = 0;
    effect->target.fields.target_id = unit->unit_id;
}

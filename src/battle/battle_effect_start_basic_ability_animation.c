#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_start_basic_ability_animation(s32 effect_id) {
    battle_effect_secondary_init_t effect_data;

    effect_data.target_count = 1;
    effect_data.caster.fields.target_type = 0;
    effect_data.caster.fields.caster_id = 0;
    effect_data.target.fields.target_type = 0;
    effect_data.target.fields.target_id = 1;
    effect_data.target.fields.result_animation = 0;
    effect_data.palette_target_count = 0;
    battle_effect_set_ability_animation(0, effect_id, &effect_data);
}

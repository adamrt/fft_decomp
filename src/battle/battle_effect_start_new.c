#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_start_new(s32 effect_id, s32 target_mode, u8* target_data) {
    battle_effect_secondary_init_t effect_data;

    effect_data.target_count = 1;
    effect_data.caster.fields.target_type = 0;
    effect_data.caster.fields.caster_id = 0;

    if (target_mode == 0) {
        effect_data.target.fields.target_type = 0;
        effect_data.target.fields.target_id = (u32)target_data;
        effect_data.target.fields.result_animation = 0;
    } else if (target_mode == 1) {
        effect_data.target.fields.target_type = 1;
        effect_data.target.block.values[1] = *(u16*)&target_data[0];
        effect_data.target.block.values[3] = *(u16*)&target_data[4];
        effect_data.target.block.values[2] = *(u16*)&target_data[2];
    } else {
        effect_data.target.fields.target_type = 0;
        effect_data.target.fields.target_id = (u32)target_data;
        effect_data.target.fields.result_animation = 0;
        effect_data.caster.fields.caster_id = (u32)target_data;
    }

    effect_data.palette_target_count = 0;
    battle_effect_set_ability_animation(0, effect_id, &effect_data);
}

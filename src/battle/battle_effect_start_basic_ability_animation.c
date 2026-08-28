#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_start_basic_ability_animation(s32 effect_id) {
    u8 effect_data[200];

    *(u16*)&effect_data[0] = 1;
    *(u16*)&effect_data[0xA4] = 0;
    effect_data[0xA6] = 0;
    *(u16*)&effect_data[4] = 0;
    effect_data[6] = 1;
    effect_data[7] = 0;
    effect_data[2] = 0;
    battle_effect_set_ability_animation(0, effect_id, (battle_effect_secondary_init_t*)effect_data);
}

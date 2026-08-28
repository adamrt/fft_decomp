#include "fft/battle.h"
#include "psx/types.h"

void battle_effect_start_new(s32 effect_id, s32 target_mode, u8* target_data) {
    u8 effect_data[200];

    *(u16*)&effect_data[0] = 1;
    *(u16*)&effect_data[0xA4] = 0;
    effect_data[0xA6] = 0;

    if (target_mode == 0) {
        *(u16*)&effect_data[4] = 0;
        effect_data[6] = (u32)target_data;
        effect_data[7] = 0;
    } else if (target_mode == 1) {
        *(u16*)&effect_data[4] = 1;
        *(u16*)&effect_data[6] = *(u16*)&target_data[0];
        *(u16*)&effect_data[0xA] = *(u16*)&target_data[4];
        *(u16*)&effect_data[8] = *(u16*)&target_data[2];
    } else {
        *(u16*)&effect_data[4] = 0;
        effect_data[6] = (u32)target_data;
        effect_data[7] = 0;
        effect_data[0xA6] = (u32)target_data;
    }

    effect_data[2] = 0;
    battle_effect_set_ability_animation(0, effect_id, (battle_effect_secondary_init_t*)effect_data);
}

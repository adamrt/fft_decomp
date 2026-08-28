#include "fft/battle.h"
#include "psx/types.h"

void battle_action_store_ability_data(u8* src) {
    s32 i = 0;
    do {
        g_battle_target_ability_targets_list[i] = *src;
        i++;
        src++;
    } while (i < 16);
}

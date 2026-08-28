#include "fft/battle.h"
#include "psx/types.h"

s32 battle_target_count_hit_by_ability(u8* out) {
    s32 count = 0;
    s32 i = 0;
    do {
        u8 target_id = g_battle_target_ability_targets_list[i];
        *out++ = target_id;
        if (target_id != 0xFF) {
            count++;
        }
        i++;
    } while (i < 0x10);
    return count;
}

#include "fft/battle.h"
#include "psx/types.h"

s32 battle_effect_is_item_ability(s32 ability_id) {
    s32 entry;
    entry = g_battle_effect_ability_ids[ability_id];
    if (entry < 0) {
        entry = ~entry;
    }
    return (entry & 0x800) > 0;
}

#include "fft/battle_ai.h"
#include "fft/unit_slots.h"

s32 battle_ai_has_any_unit_decided_to_use_ability(void) {
    battle_ai_data_t* ai;
    s32 result;
    s32 i;

    result = 0;
    ai = &g_battle_ai_data_base;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        if (ai->targetability.live.unit_targetable[i] != 0) {
            if (battle_ai_classify_ability_effect(i, 1) == BATTLE_AI_ABILITY_EFFECT_USEFUL) {
                result = 1;
            } else {
                ai->targetability.live.unit_targetable[i] = 0;
            }
        }
    }
    return result;
}

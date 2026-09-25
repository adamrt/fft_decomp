#include "fft/battle.h"

s32 battle_unit_apply_level_up_down_ability(void) {
    s32 lv;
    s32 dir;
    u16 flags;

    flags = g_battle_action_target_data->special_effect;
    if (flags & BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_UP) {
        lv = g_battle_action_target->level + 1;
        dir = 0;
        if (lv >= 100) {
            lv = 99;
        }
    } else if (flags & BATTLE_ACTION_SPECIAL_EFFECT_LEVEL_DOWN) {
        lv = g_battle_action_target->level - 1;
        dir = 1;
        if (lv <= 0) {
            lv = 1;
        }
    } else {
        return 0;
    }
    if (g_battle_action_state == BATTLE_ACTION_STATE_EXECUTE) {
        g_battle_action_target->level = lv;
        main_unit_apply_level_growth(g_battle_action_target, dir);
    }
    return 1;
}

#include "fft/battle_action.h"
#include "fft/battle_unit.h"

/* Apply a level-up or level-down special effect: level + 1 (99 at most) or
 * level - 1 (1 at least), then one level of growth or its inverse. The new
 * level is stored before the growth runs, so the growth divides by growth +
 * new level. The unit changes only when executing; returns 1 when the action
 * carries either effect. */
s32 battle_unit_apply_level_up_down_ability(void) {
    s32 lv;
    s32 dir;
    u16 flags;

    flags = g_action_target_data->special_effect;
    if (flags & ACTION_SPECIAL_EFFECT_LEVEL_UP) {
        lv = g_action_target->level + 1;
        dir = 0;
        if (lv > UNIT_LEVEL_MAX) {
            lv = UNIT_LEVEL_MAX;
        }
    } else if (flags & ACTION_SPECIAL_EFFECT_LEVEL_DOWN) {
        lv = g_action_target->level - 1;
        dir = 1;
        if (lv <= 0) {
            lv = 1;
        }
    } else {
        return 0;
    }
    if (g_action_state == ACTION_STATE_EXECUTE) {
        g_action_target->level = lv;
        main_unit_apply_level_growth(g_action_target, dir);
    }
    return 1;
}

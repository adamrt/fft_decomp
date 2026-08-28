#include "fft/battle.h"
#include "psx/types.h"

s32 battle_action_check_reaction(battle_stats_t* unit) {
    s32 result;
    /* Both globals are loaded unsigned (lhu) here. */
    if (g_current_ability.reaction_id != 0) {
        if (g_current_ability.elemental_flags == 0)
            return 1;
    }
    /* The target loads the reaction id signed (lh). */
    result = *(s16*)&unit->action.reaction_id;
    if (result != 0)
        result = 2;
    return result;
}

#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_clear_nullify_flags(void) {
    u16* pflag;
    battle_action_data_t* action;
    u16* special_effect;
    pflag = &g_current_ability.elemental_flags;
    if (*pflag != 0) {
        action = g_battle_action_target_data;
        g_current_ability.reaction_id = 0;
        *pflag = 0;
        /* Clear the nullify bit (0x0200).  Reading the field through a
         * second pointer keeps the lhu below the two scalar global stores;
         * a direct action->special_effect load is hoisted above them
         * (MEM_IN_STRUCT_P) and the function comes out 4 bytes short. */
        special_effect = &action->special_effect;
        *special_effect &= ~BATTLE_ACTION_SPECIAL_EFFECT_NULLIFIED;
    }
}

#include "fft/battle.h"

void battle_formula_apply_attack_up_and_martial_arts(void) {
    if (g_battle_action_attacker->support_abilities[1] & BATTLE_SUPPORT_SET_2_ATTACK_UP) {
        s16* xa = (s16*)&g_current_ability.xa;

        *xa = *xa * 4 / 3;
    }

    {
        u8 weapon_id = g_current_ability.weapon_id;
        battle_stats_t* attacker = g_battle_action_attacker;
        u8 weapon_type = g_main_item_primary_data[weapon_id].type;
        u8 has_martial_arts = attacker->support_abilities[2] & BATTLE_SUPPORT_SET_3_MARTIAL_ARTS;

        if (has_martial_arts && weapon_type == ITEM_TYPE_UNARMED) {
            g_current_ability.xa = (s16)g_current_ability.xa * 3 / 2;
        }
    }
}

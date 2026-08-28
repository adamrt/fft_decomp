#include "fft/battle.h"
#include "fft/data.h"
#include "fft/main_runtime.h"

void battle_formula_init_weapon_xa_ya(void) {
    u8 weapon_type = g_main_item_primary_data[g_current_ability.weapon_id].type;

    if (weapon_type == ITEM_TYPE_POLEARM) {
        g_current_ability.xa = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK] * 3 / 2;
    } else {
        g_current_ability.xa = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    }

    if (weapon_type == ITEM_TYPE_UNARMED) {
        s16* ya = (s16*)&g_current_ability.ya;
        s32 value;

        value = (s16)g_current_ability.xa * g_battle_action_attacker->brave / 100;
        *ya = value;
        if ((s16)value == 0) {
            *ya = value + 1;
        }
    } else {
        g_current_ability.ya = g_current_ability.weapon_data.power;
    }
}

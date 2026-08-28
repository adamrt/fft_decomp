#include "fft/battle.h"
#include "psx/types.h"

void battle_move_apply_knockback(void) {
    u8* flags;
    battle_stats_t* unit;
    battle_action_data_t* action;
    s32 amount;
    s32 diff;

    battle_action_clear_data();
    flags = &g_current_ability.knockback_flags;
    *flags &= 0x7F;
    g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_PSEUDO_STATUS;
    unit = g_battle_action_target;
    amount = 0;
    if ((*(u16*)&unit->status_sets.current[0]
            & (BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_CRYSTAL) | BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_DEAD)
                | BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_PETRIFY)))
        == 0) {
        if (*flags == 1) {
            diff = (g_current_ability.knockback_fall_height >> 1) - unit->jump;
            if (diff > 0) {
                amount = (diff * unit->max_hp) / 10;
                if (amount >= 0x3E8) {
                    amount = 0x3E7;
                }
            }
        }
        action = g_battle_action_target_data;
        action->hp_damage = amount;
        if (amount != 0) {
            action->attack_type |= BATTLE_ACTION_TYPE_HP_DAMAGE;
        }
    }
}

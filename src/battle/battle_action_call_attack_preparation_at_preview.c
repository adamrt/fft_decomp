#include "fft/battle.h"
#include "psx/types.h"

s32 battle_action_call_attack_preparation_at_preview(u8* command) {
    battle_stats_t* unit;
    s32 result;

    unit = &g_battle_unit_stats[*command];
    result = battle_action_prepare_attack(
        (battle_ai_command_action_t*)command, (battle_ai_command_action_t*)&unit->action_actor_id, 1);
    if (result == 1 && g_battle_action_context == BATTLE_ACTION_CONTEXT_PRIMARY) {
        battle_status_enable_disable_acting(unit);
    }
    return result;
}

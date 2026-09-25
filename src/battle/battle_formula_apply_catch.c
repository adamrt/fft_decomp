#include "fft/battle.h"
#include "psx/types.h"

/* Catch the thrown weapon when the target's Brave-based reaction roll succeeds. */
void battle_formula_apply_catch(void) {
    battle_stats_t* unit;
    battle_action_data_t* action;
    /* Loaded through a pointer so GCC keeps it between the struct stores
     * (a plain scalar load is assumed not to alias them and floats). */
    u8* weapon_id_ptr = &g_current_ability.weapon_id;
    s32 weapon_id;
    s32 chance;

    if (battle_action_can_unit_react_1(g_battle_action_target) != 0)
        return;
    if (g_battle_action_attacker->job_id == JOB_ID_MIME)
        return;
    unit = g_battle_action_target;
    if ((unit->reaction_abilities[3] & BATTLE_REACTION_SET_4_CATCH) == 0)
        return;
    chance = 0x64 - unit->brave;
    /* The target stores the accuracy as a halfword (0x2a..0x2b). */
    g_battle_action_target_data->attack_accuracy = (s16)chance;
    if (chance != 0) {
        if (battle_action_calculate_chance_to_react(unit) != 0)
            return;
        if (g_battle_action_state != BATTLE_ACTION_STATE_EXECUTE)
            return;
    }
    g_battle_action_target_data->hit = 0;
    action = g_battle_action_target_data;
    action->reaction_id = ABILITY_ID_REACTION_CATCH;
    weapon_id = *weapon_id_ptr;
    action->miss_type = BATTLE_ACTION_MISS_TYPE_CATCH;
    action->last_received_attack = (s16)weapon_id;
}

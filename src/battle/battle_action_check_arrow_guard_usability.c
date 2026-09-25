#include "fft/battle.h"
#include "psx/types.h"

/* Applies Arrow Guard to bow and crossbow attacks that use weapon range.
 *
 * A successful Brave-based reaction changes the target result to the shared
 * class-evade/Arrow Guard miss type. */
void battle_action_check_arrow_guard_usability(void) {
    battle_stats_t* target;
    battle_action_data_t* action;

    if ((g_current_ability.range_data.flags_1 & ABILITY_SECONDARY_FLAG_1_WEAPON_RANGE) == 0) {
        return;
    }
    if ((u32)(g_main_item_primary_data[g_current_ability.weapon_id].type - ITEM_TYPE_CROSSBOW) >= 2) {
        return;
    }
    target = g_battle_action_target;
    action = g_battle_action_target_data;
    /* The accuracy field is written as a halfword (sh) here. */
    action->attack_accuracy = 100 - target->brave;
    if (battle_action_calculate_chance_to_react(g_battle_action_target) == 0
        && g_battle_action_state == BATTLE_ACTION_STATE_EXECUTE) {
        g_battle_action_target_data->hit = 0;
        g_battle_action_target_data->miss_type = BATTLE_ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD;
        g_battle_action_target_data->reaction_id = ABILITY_ID_REACTION_ARROW_GUARD;
    }
}

#include "fft/battle.h"
#include "psx/types.h"

/* Preview where a planned action lands on the AT list.
 *
 * Runs the attack preparation for action in the preview state, rebuilds
 * at_list and counts the turns until the action resolves, then restores the
 * unit's action data, CT, move/act flags and statuses. */
s32 battle_action_preview_at_list(battle_stats_t* unit, s32 action, s32 at_list) {
    u8 saved_action[0x18];
    u8* action_data;
    s32 old_state;
    s32 turns;
    u8 old_charged_ability_ct;
    u8 old_inflicted_status;
    u8 old_movement_taken;
    u8 old_action_taken;

    action_data = &unit->action_actor_id;
    main_util_copy_action_data(action_data, saved_action);
    old_charged_ability_ct = unit->charged_ability_ct;
    old_inflicted_status = unit->inflicted_status[0];
    old_movement_taken = unit->movement_taken;
    old_action_taken = unit->action_taken;
    old_state = g_battle_action_state;
    g_battle_action_state = BATTLE_ACTION_STATE_PREVIEW;
    if (battle_action_call_attack_preparation_at_preview((u8*)action) == 1) {
        unit->charged_ability_ct = 0;
    }
    turns = battle_action_calculate_at_list((battle_at_entry_t*)at_list, 3);
    if (turns != -1) {
        turns = battle_action_get_number_of_turns_to_resolve(
                    unit->misc_unit_id, unit->charged_ability_ct, (battle_at_entry_t*)at_list)
            & 0xFF;
    }
    g_battle_action_state = old_state;
    unit->movement_taken = old_movement_taken;
    unit->action_taken = old_action_taken;
    unit->inflicted_status[0] = old_inflicted_status;
    main_status_store_current(unit);
    unit->charged_ability_ct = old_charged_ability_ct;
    main_util_copy_action_data(saved_action, action_data);
    return turns;
}

#include "fft/battle_ai.h"
#include "fft/main_runtime.h"

/*
 * Select an action for the retreat-oriented AI path.
 *
 * Try abilities targeting the actor and an existing charge before the broader
 * ability pass. Return -1 to suspend and 0 when selection completes.
 */
s32 battle_ai_select_retreat_action(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_ai_ability_entry_t* entry;
    s32 i, result;

    if (g_battle_ai_data_base.decision_state) {
        /* Resume: re-enter the step that suspended. */
        switch (g_battle_ai_retreat_phase) {
        case 0:
            goto movement;
        case 1:
            goto self_abilities;
        case 2:
            goto charging;
        default:
            goto other_abilities;
        }
    }
    battle_ai_store_main_target_id_and_focus_on_target_flag(
        battle_ai_find_nearest_target(BATTLE_AI_NEAREST_HEALER_OR_CRYSTAL));
movement:
    if (battle_ai_run_cowardly_movement() == -1) {
        g_battle_ai_retreat_phase = 0;
        return -1;
    }
    ai->movement_scenario = 1;
    battle_ai_invert_target_priority();
    if (battle_ai_select_active_units(BATTLE_AI_UNIT_FILTER_SPECIFIC, ai->acting_unit_id)) {
    self_abilities:
        if (battle_ai_choose_move_from_move_list() == -1) {
            g_battle_ai_retreat_phase = 1;
            return -1;
        }
    }
    if (battle_ai_select_ranked_action())
        return 0;
charging:
    result = battle_ai_evaluate_charging_movement();
    switch (result) {
    case -1:
        g_battle_ai_retreat_phase = 2;
        return -1;
    case 1:
        return 0;
    }
    for (i = 0; i < 34; i++) {
        entry = &ai->ability_lists[ai->acting_unit_battle_id][i];
        if (entry->skillset_flags.bytes.skillset == SKILLSET_ID_NONE)
            break;
        battle_ai_load_ability_entry(entry);
        /* The aligned skillset/usage pair preserves the target's LHU/SH. */
        if (g_main_action_menu_types_by_skillset[entry->skillset_flags.bytes.skillset] == ACTION_MENU_TYPE_CHARGE)
            entry->skillset_flags.packed &= 0x7fff;
    }
    if (battle_ai_select_active_units(BATTLE_AI_UNIT_FILTER_ANY, 255)) {
    other_abilities:
        if (battle_ai_choose_move_from_move_list() == -1) {
            g_battle_ai_retreat_phase = 3;
            return -1;
        }
        if (battle_ai_select_ranked_action())
            return 0;
    }
    battle_ai_transfer_ability_data_and_set_defend_flag();
    return 0;
}

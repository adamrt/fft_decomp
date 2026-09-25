#include "fft/battle.h"

/* Starts the acting unit's confirmed ability: marks the action taken, faces the
 * cursor and runs the attack preview. A preview result of 1 or 3 enters the
 * commence-attack phase; 0 starts the charge animation. */
void battle_action_execute_ability(void) {
    battle_unit_misc_data_t* unit;
    s32 result;

    battle_menu_store_unit_names_and_event_block_data(0, 0, 0);
    unit = battle_unit_get_source_misc_data();
    battle_action_set_only_action_taken(unit->battle_data->misc_unit_id);
    unit->ability_ct_resolved |= 2;
    battle_unit_face_toward_cursor(unit);
    result = battle_action_call_attack_preparation_at_preview((u8*)&unit->command_state.ai.data);
    unit->attack_phase_state = result;
    switch (result) {
    case 0:
        battle_unit_start_ability_charge_animation_for_movement(unit);
        break;
    case 1:
    case 3:
        battle_state_stop_game_flow();
        g_battle_game_state = BATTLE_GAME_STATE_COMMENCE_ATTACK_PHASE;
        g_battle_action_post_action = 0;
        unit->ability_ct_resolved |= 2;
        if (g_battle_ability_animation_data[unit->used_ability_id].charge_animation_set_id != 0) {
            battle_unit_start_ability_charge_animation_for_movement(unit);
        }
        return;
    case 2:
    default:
        main_system_handle_pointer_exception(0x13);
        break;
    }
    battle_unit_update_post_command_animation_display(unit);
}

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"

/* Starts the acting unit's confirmed ability: marks the action taken, faces the
 * cursor and runs the attack preview. A preview result of 1 or 3 enters the
 * commence-attack phase; 0 starts the charge animation. */
void action_execute_ability(void) {
    battle_unit_t* unit;
    s32 result;

    menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0, 0);
    unit = battle_unit_get_source();
    action_set_only_action_taken(unit->stats->battle_id);
    unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
    battle_unit_face_toward_cursor(unit);
    result = action_call_attack_preparation_at_preview(&unit->command_state.ai.data.action);
    unit->attack_phase_state = result;
    switch (result) {
    case BATTLE_ATTACK_PREPARATION_CHARGE:
        battle_unit_start_ability_charge_hold_animation(unit);
        break;
    case BATTLE_ATTACK_PREPARATION_IMMEDIATE:
    case BATTLE_ATTACK_PREPARATION_JUMP:
        camera_disable_map_pan();
        g_battle_game_state = BATTLE_GAME_STATE_COMMENCE_ATTACK_PHASE;
        g_action_post_action = 0;
        unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
        if (g_battle_ability_animation_data[unit->used_ability_id].charge_animation_set_id != 0) {
            battle_unit_start_ability_charge_hold_animation(unit);
        }
        return;
    case BATTLE_ATTACK_PREPARATION_CHANGE_EQUIPMENT:
    default:
        main_system_handle_pointer_exception(0x13);
        break;
    }
    battle_unit_update_post_command_animation_display(unit);
}

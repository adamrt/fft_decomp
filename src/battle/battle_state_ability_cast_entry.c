#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/* Enter the open-SP2-files state at animation speed 1 with no SP2 buffer
 * allocated. */
void battle_state_enter_open_sp2_files(void) {
    g_main_system_animation_speed = 1;
    g_battle_game_state = BATTLE_GAME_STATE_OPEN_SP2_FILES;
    g_battle_gfx_sp2_data = 0;
}

/* Enter the pre-attack animation state: clear the tile marks and start the used
 * ability's charge animation, or the standing animation when the ability has no
 * charge animation set. */
void battle_state_enter_pre_attack_animation(void) {
    battle_unit_t* unit;
    u16 ability;

    target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 2);
    g_battle_game_state = BATTLE_GAME_STATE_PRE_ATTACK_ANIMATION;
    unit = battle_unit_get_source();
    ability = unit->used_ability_id;
    unit->state_frame_counter = 0;
    if (g_battle_ability_animation_data[ability].charge_animation_set_id != 0) {
        battle_unit_start_ability_charge_animation(unit);
    } else {
        /* facing is read signed (lh) here. */
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_STAND, unit->facing, unit);
    }
    action_clear_at_list_active();
}

/* Enter the action-cast state for the source unit, unless the scenario check
 * starts an event first; the camera turns when the unit's tile is hidden. */
void battle_state_enter_action_cast(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ACTION_CAST;
    g_action_post_action = 0;
    unit = battle_unit_get_source();
    if (battle_state_try_enter_scenario_event(unit) == 0) {
        g_battle_game_state = BATTLE_GAME_STATE_ACTION_CAST;
        camera_rotate_when_unit_tile_not_visible(unit);
    }
}

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

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_target.h"
#include "psx/pad.h"
#include "psx/types.h"

/* IdlingActionMenus game-state handler: act on the selected menu command.
 *
 * Move (0) and the 0x19 variant hand off to the move command, Act (1)
 * dispatches Defend, Equip Change and ordinary abilities on the command's
 * skillset, Wait (2) chooses a facing, and the remaining commands enter
 * target selection/display, the event check or scripted menus.
 *
 * The kind is switched as s32: the unsigned enum type makes GCC emit sltiu
 * where the target has slti. The empty cases for the unresolved commands 0xfe
 * and 0xff keep the switch a compare tree with the target's split: a
 * two-value range above 0x19 disables the jump table (range > 10 * count). */
void battle_state_handle_idling_action_menus_state(void) {
    ai_command_t* command;
    battle_unit_t* unit;
    s32 result;

    if (menu_get_thread_activity_variant() == 0) {
        battle_state_set_free_cursor();
    }
    if (g_main_input_pressed_buttons & PSX_PAD_SELECT) {
        battle_menu_open_help();
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    command = menu_get_selected_command_address();
    unit = battle_unit_get_casting();
    switch ((s32)command->kind) {
    case AI_COMMAND_MOVE:
        if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_NONE) {
            g_battle_casting_runtime_id = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id)->runtime_id;
        } else {
            g_battle_casting_runtime_id = g_battle_unit_source_runtime_id;
        }
        action_clear_at_list_id();
        action_handle_move_command();
        break;
    case AI_COMMAND_MOVE_SELF:
        g_battle_casting_runtime_id = g_battle_unit_source_runtime_id;
        action_clear_at_list_id();
        action_handle_move_command();
        break;
    case AI_COMMAND_ACT:
        action_clear_at_list_id();
        switch (command->data.action.skillset_id) {
        case SKILLSET_ID_DEFEND:
            unit->command_state.ai = *command;
            status_inflict_defending_to_battle_id(unit->stats->battle_id);
            unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            action_set_only_action_taken(unit->stats->battle_id);
            battle_state_enter_after_command();
            break;
        case SKILLSET_ID_EQUIP_CHANGE:
            unit->command_state.ai = *command;
            unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_ACTED;
            status_enable_disable_acting(unit->stats);
            ai_init_unit_abilities(unit->stats->battle_id);
            battle_unit_update_display_by_runtime_id(unit->runtime_id);
            action_set_only_action_taken(unit->stats->battle_id);
        case SKILLSET_ID_BLANK_04:
        case SKILLSET_ID_MIMIC:
            battle_state_enter_after_command();
            break;
        case SKILLSET_ID_ATTACK:
        default:
            unit->command_state.ai = *command;
            unit->command_state.ai.data.action.actor_id = unit->stats->battle_id;
            unit->command_state.ai.data.action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
            action_call_attack_preparation(&unit->command_state.ai.data.action);
            unit->used_ability_id = unit->command_state.ai.data.action.ability_id;
            result = target_set_panels_for_action(&unit->command_state.ai.data.action);
            unit->ability_preview_phase = result;
            switch (result) {
            case TARGET_PANELS_UNIT_OR_TILE:
            case TARGET_PANELS_TILE_ONLY:
            case TARGET_PANELS_NONE:
                battle_state_enter_action_execution_setup();
                break;
            case TARGET_PANELS_NO_TARGETING:
            default:
                target_select_tile();
                break;
            }
            break;
        }
        break;
    case AI_COMMAND_END_TURN:
        action_choose_wait();
        break;
    case AI_COMMAND_AUTOBATTLE_TARGETED_SELECT:
    case AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT:
        unit->target_select_command = command->kind;
        battle_state_enter_target_select_start();
        break;
    case AI_COMMAND_AUTOBATTLE_TARGETED_DISPLAY:
    case AI_COMMAND_AUTOBATTLE_RECOVERY_DISPLAY:
        unit->target_select_command = command->kind;
        battle_state_enter_target_display_start();
        break;
    case AI_COMMAND_AUTOBATTLE_FADING_LIFE:
    case AI_COMMAND_AUTOBATTLE_RETREAT:
        battle_menu_set_next_script_action_menus();
        break;
    case AI_COMMAND_NO_MENU_RECORD:
        battle_menu_clear_status_menu_state();
        action_check_between_turn_events();
        break;
    case AI_COMMAND_REFRESH_CONTROL:
        battle_unit_store_entd_flags_without_control_flag(unit);
        break;
    case AI_COMMAND_UNRESOLVED_FE:
    case AI_COMMAND_UNRESOLVED_FF:
        break;
    }
}

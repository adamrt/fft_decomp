#include "fft/battle.h"

/*
 * Advance menu-to-targeting state and consume the unit's renderer command.
 *
 * AI setup may suspend before the command is ready. Keep each special
 * skillset's completion explicit so its distinct call tail is preserved.
 */
void battle_state_handle_menu_to_targeting_state(void) {
    battle_unit_misc_data_t* unit;
    s32 kind;
    s32 result;
    u16 frame;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    unit = battle_unit_get_source_misc_data();
    if (unit->state_frame_counter >= 31 && !unit->command_ready) {
        g_animation_speed = 2;
        if (!battle_ai_build_command(unit->battle_data->misc_unit_id, &unit->command_state.ai)) {
            if (unit->command_state.ai.kind == BATTLE_AI_COMMAND_ACT) {
                unit->command_state.ai.data.action.unit_id = unit->battle_data->misc_unit_id;
                battle_action_call_attack_preparation(&unit->command_state.ai.data.action);
            }
            unit->command_ready = 1;
            g_animation_speed = 1;
        }
    }
    frame = unit->state_frame_counter++;
    if (frame < 31 || !unit->command_ready)
        return;
    kind = unit->command_state.ai.kind;
    switch (kind) {
    case BATTLE_AI_COMMAND_MOVE:
        if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_NONE) {
            g_battle_casting_unit_id = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id)->unit_id;
            battle_action_handle_move_command();
            return;
        }
        /* fallthrough */
    case 0x19:
        g_battle_casting_unit_id = g_battle_casting_misc_id;
        battle_action_handle_move_command();
        return;
    case BATTLE_AI_COMMAND_ACT:
        switch (unit->command_state.ai.data.action.skillset) {
        case SKILLSET_ID_DEFEND:
            battle_status_inflict_defending_to_battle_id(unit->battle_data->misc_unit_id);
            unit->ability_ct_resolved |= 2;
            battle_unit_update_display_by_misc_id(unit->unit_id);
            battle_action_set_only_action_taken(unit->battle_data->misc_unit_id);
            battle_state_enter_after_command();
            return;
        case SKILLSET_ID_EQUIP_CHANGE:
            unit->ability_ct_resolved |= 2;
            battle_status_enable_disable_acting(unit->battle_data);
            battle_ai_init_unit_abilities(unit->battle_data->misc_unit_id);
            battle_unit_update_display_by_misc_id(unit->unit_id);
            battle_action_set_only_action_taken(unit->battle_data->misc_unit_id);
            battle_state_enter_after_command();
            return;
        case SKILLSET_ID_BLANK_04:
        case SKILLSET_ID_MIMIC:
            battle_action_set_only_action_taken(unit->battle_data->misc_unit_id);
            battle_state_enter_after_command();
            return;
        /* Explicit ordinary skillsets retain the native 24-entry dispatch table. */
        case SKILLSET_ID_ATTACK:
        case SKILLSET_ID_BASIC_SKILL:
        case SKILLSET_ID_ITEM:
        case SKILLSET_ID_BATTLE_SKILL:
        case SKILLSET_ID_CHARGE:
        case SKILLSET_ID_PUNCH_ART:
        case SKILLSET_ID_WHITE_MAGIC:
        case SKILLSET_ID_BLACK_MAGIC:
        case SKILLSET_ID_TIME_MAGIC:
        case SKILLSET_ID_SUMMON_MAGIC:
        case SKILLSET_ID_STEAL:
        case SKILLSET_ID_TALK_SKILL:
        case SKILLSET_ID_YIN_YANG_MAGIC:
        case SKILLSET_ID_ELEMENTAL:
        case SKILLSET_ID_JUMP:
        case SKILLSET_ID_DRAW_OUT:
        case SKILLSET_ID_THROW:
        case SKILLSET_ID_MATH_SKILL:
        case SKILLSET_ID_SING:
        case SKILLSET_ID_DANCE:
        default:
            unit->used_ability_id = unit->command_state.ai.data.action.ability_id;
            result = battle_target_set_panels_for_action(&unit->command_state.ai.data.action.unit_id);
            unit->ability_preview_phase = result;
            switch (result) {
            case 0:
            case 1:
            case 3:
                battle_state_enter_action_execution_setup();
                return;
            case 2:
            default:
                battle_target_select_tile();
                return;
            }
        }
    case BATTLE_AI_COMMAND_END_TURN:
        break;
    default:
        return;
    }
    unit->ability_ct_resolved |= 3;
    battle_action_end_turn(unit->battle_data->misc_unit_id);
    battle_state_enter_after_command();
}

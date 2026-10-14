#include "fft/battle.h"
#include "fft/battle_target.h"

/* ConfirmAction game-state handler: act on the answer to the prompt
 * action_confirm opened.
 *
 * For a player-controlled unit command 5 targets the cursor tile; 6 and 7 target
 * the unit on it, storing the command as the targeting type and the unit's battle
 * id, or fall back to targeting type 5 when no unit stands there. All three store
 * the cursor tile as the target panel and enter the pre-attack animation. Command
 * 8 or 0xff clears the tile marks and returns to the range display. An
 * AI-controlled unit keeps the target it already chose: 5, 6 and 7 only enter the
 * pre-attack animation. */
void battle_state_handle_confirm_action_state(void) {
    battle_unit_t* unit;
    ai_command_t* selected_command_address;
    battle_unit_t* target;
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    unit = battle_unit_get_source();
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        selected_command = selected_command_address->kind;
        switch (selected_command) {
        case 5:
            unit->command_state.ai.data.action.targeting_type = selected_command_address->kind;
            main_util_set_svector(
                &unit->command_state.cursor.target_panel, g_battle_cursor_x, g_battle_cursor_z, g_battle_cursor_y);
            battle_state_enter_pre_attack_animation();
            return;
        case 6:
        case 7:
            target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
            if (target != 0) {
                unit->command_state.ai.data.action.targeting_type = selected_command_address->kind;
                unit->command_state.ai.data.action.target_id = target->stats->battle_id;
            } else {
                unit->command_state.ai.data.action.targeting_type = BATTLE_TARGETING_TYPE_TILE;
            }
            main_util_set_svector(
                &unit->command_state.cursor.target_panel, g_battle_cursor_x, g_battle_cursor_z, g_battle_cursor_y);
            battle_state_enter_pre_attack_animation();
            return;
        case 8:
        case 0xff:
            target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 3);
            target_set_boxes_red();
            return;
        default:
            break;
        }
    } else {
        selected_command = selected_command_address->kind;
        switch (selected_command) {
        case 5:
        case 6:
        case 7:
            battle_state_enter_pre_attack_animation();
            return;
        case 8:
        case 0xff:
            target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 3);
            target_set_boxes_red();
            return;
        default:
            break;
        }
    }
}

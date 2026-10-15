#include "fft/battle.h"
#include "fft/battle_target.h"
#include "psx/pad.h"

/*
 * Per-frame handler for the BattleTargetSelectMode game state.
 *
 * Circle accepts the unit under the cursor. Selected ability 0xc requires a
 * unit outside the caster's non-player team and 0xe one inside it; any other
 * value leaves the selection pending. X returns to the caster and the script
 * action menus; triangle only recenters the cursor on the caster.
 */
void battle_state_handle_target_select_state(void) {
    battle_unit_t* caster;
    battle_unit_t* target;

    camera_handle_rotation_input();
    camera_update_zoom_input();
    camera_update_tilt_input();
    target_move_cursor_by_input();
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    if (g_main_input_pressed_buttons & PSX_PAD_CIRCLE) {
        caster = battle_unit_get_casting();
        target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        /* The targeted choice requires an enemy, the recovery choice an ally. */
        if (target == 0) {
            battle_state_enter_target_select_denied();
        } else if (caster->target_select_command == AI_COMMAND_AUTOBATTLE_TARGETED_SELECT) {
            if ((caster->stats->team_flags & BATTLE_TEAM_MASK) == (target->stats->team_flags & BATTLE_TEAM_MASK)) {
                battle_state_enter_target_select_denied();
            } else {
                battle_state_enter_target_select_confirm();
            }
        } else if (caster->target_select_command == AI_COMMAND_AUTOBATTLE_RECOVERY_SELECT) {
            if ((caster->stats->team_flags & BATTLE_TEAM_MASK) == (target->stats->team_flags & BATTLE_TEAM_MASK)) {
                battle_state_enter_target_select_confirm();
            } else {
                battle_state_enter_target_select_denied();
            }
        }
        battle_gfx_update_unit_palettes();
    } else if (g_main_input_pressed_buttons & PSX_PAD_CROSS) {
        target_move_cursor_to_unit(battle_unit_get_casting());
        battle_gfx_update_unit_palettes();
        battle_menu_set_next_script_action_menus();
    } else if (g_main_input_pressed_buttons & PSX_PAD_TRIANGLE) {
        target_move_cursor_to_unit(battle_unit_get_casting());
    }
}

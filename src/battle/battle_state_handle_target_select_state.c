#include "fft/battle.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Per-frame handler for the BattleTargetSelectMode game state.
 *
 * Circle accepts the unit under the cursor. Selected ability 0xc requires a
 * unit outside the caster's non-player team and 0xe one inside it; any other
 * value leaves the selection pending. X returns to the caster and the script
 * action menus; triangle only recentres the cursor on the caster.
 */
void battle_state_handle_target_select_state(void) {
    battle_unit_misc_data_t* caster;
    battle_unit_misc_data_t* target;

    battle_camera_handle_rotation_input();
    battle_camera_call_zoom_map();
    battle_camera_call_toggle_tilt();
    battle_target_move_cursor_by_input();
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    if (g_controller_input_pressed & PSX_PAD_CIRCLE) {
        caster = battle_unit_get_casting_misc_data();
        target = battle_unit_get_selectable_misc_data_at_map_coords(
            g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        /* 0x0c requires an enemy target, 0x0e an ally. */
        if (target == 0) {
            battle_state_enter_target_select_denied();
        } else if (caster->target_select_command == 0xC) {
            if ((caster->battle_data->team_flags & BATTLE_TEAM_MASK)
                == (target->battle_data->team_flags & BATTLE_TEAM_MASK)) {
                battle_state_enter_target_select_denied();
            } else {
                battle_state_enter_target_select_confirm();
            }
        } else if (caster->target_select_command == 0xE) {
            if ((caster->battle_data->team_flags & BATTLE_TEAM_MASK)
                == (target->battle_data->team_flags & BATTLE_TEAM_MASK)) {
                battle_state_enter_target_select_confirm();
            } else {
                battle_state_enter_target_select_denied();
            }
        }
        battle_gfx_update_unit_palettes();
    } else if (g_controller_input_pressed & PSX_PAD_CROSS) {
        battle_target_move_cursor_to_unit(battle_unit_get_casting_misc_data());
        battle_gfx_update_unit_palettes();
        battle_menu_set_next_script_action_menus();
    } else if (g_controller_input_pressed & PSX_PAD_TRIANGLE) {
        battle_target_move_cursor_to_unit(battle_unit_get_casting_misc_data());
    }
}

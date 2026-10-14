#include "fft/battle.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/* TargetSelectConfirm game-state handler: command 7 stores the unit under the
 * cursor as the casting unit's auto-battle target and opens the next action
 * menus; 8 or 0xff returns to target selection. */
void battle_state_handle_target_select_confirm_state(void) {
    battle_unit_t* caster;
    battle_unit_t* target;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    switch ((s32)menu_get_selected_command_address()->kind) {
    case 7:
        caster = battle_unit_get_casting();
        target = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        menu_store_auto_battle_main_target_id(target->stats->battle_id, caster->stats->battle_id);
        target_move_cursor_to_unit(caster);
        battle_menu_set_next_script_action_menus();
        break;
    case 8:
    case 0xFF:
        battle_state_enter_target_select();
        break;
    }
}

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* BattleMessageDisplay game-state handler: when the message closes (command 7, 8
 * or 0xff), queue and show the casting unit's post-effect messages. */
void battle_state_handle_battle_message_display_state(void) {
    s32 selected_command;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command = menu_get_selected_command_address()->kind;
    if (selected_command >= 7 && (selected_command < 9 || selected_command == 0xff)) {
        g_action_post_action = 1;
    }
    if (g_action_post_action != 0) {
        action_show_caster_post_effect_messages();
    }
}

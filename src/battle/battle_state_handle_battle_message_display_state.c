#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_state_handle_battle_message_display_state(void) {
    s32 selected_ability;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    selected_ability = *battle_menu_get_selected_ability_address();
    if (selected_ability >= 7 && (selected_ability < 9 || selected_ability == 0xff)) {
        g_battle_action_post_action = 1;
    }
    if (g_battle_action_post_action != 0) {
        battle_action_show_caster_post_effect_messages();
    }
}

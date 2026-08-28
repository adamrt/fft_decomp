#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"

void battle_state_handle_after_command_state(void) {
    s32* ability_addr;
    s32 ability;
    battle_unit_misc_data_t* unit;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    ability_addr = battle_menu_get_selected_ability_address();
    unit = battle_unit_get_source_misc_data();
    ability = *ability_addr;
    if ((ability >= 7) && ((ability < 9) || (ability == 0xFF))) {
        g_battle_action_post_action = 1;
    }
    if ((g_battle_action_post_action != 0) && (unit->numeric_display_active == 0)) {
        battle_state_enter_jp_xp_gain();
    }
}

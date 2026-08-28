#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"

void battle_state_handle_ability_preview_help_state(void) {
    if (battle_menu_is_still_building() != 2) {
        g_battle_menu_help_opening = 0;
        battle_state_start_game_flow();
        g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING;
    }
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
}

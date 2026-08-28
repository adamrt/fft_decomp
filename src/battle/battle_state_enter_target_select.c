#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "psx/types.h"

void battle_state_enter_target_select(void) {
    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT;
    battle_target_store_cursor_unit_name_and_data();
    battle_gfx_highlight_all_units_blue_or_red(battle_unit_get_casting_misc_data()->target_select_command);
}

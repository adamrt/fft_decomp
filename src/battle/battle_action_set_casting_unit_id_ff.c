#include "fft/battle.h"

void battle_action_set_casting_unit_id_ff(void) {
    g_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_EVENT;
    battle_gfx_reset_jumping_unit_graphic_triggers();
    battle_script_reset_event_state();
    battle_action_clear_at_list_active();
    g_battle_menu_status_enabled = 0;
    g_battle_casting_unit_id = 0xff;
    g_battle_casting_misc_id = 0xff;
}

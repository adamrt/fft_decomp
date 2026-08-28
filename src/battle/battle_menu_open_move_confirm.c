#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_menu_open_move_confirm(void) {
    battle_unit_misc_data_t* source;
    battle_unit_misc_data_t* caster;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_MOVE_CONFIRM_MENU;
    source = battle_unit_get_source_misc_data();
    caster = battle_unit_get_casting_misc_data();
    battle_text_set_message_duration_frames(0x3C);
    if (caster->movement_flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT) {
        if (caster->last_path_count == 0xFF) {
            battle_menu_init_system_function(0xA, 0x183B, source->battle_data->misc_unit_id, 0, 1);
        } else {
            battle_menu_init_system_function(0xD, 0, source->battle_data->misc_unit_id, 0, 0);
        }
        caster->last_path_count = 0;
    } else {
        if (g_main_game_options.fields.navigation_messages != GAME_OPTION_ON) {
            battle_gfx_modify_palette_based_on_team(source, 2);
            battle_menu_init_system_function(
                0x10, 0, source->battle_data->misc_unit_id, 0, source->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            battle_gfx_modify_palette_based_on_team(source, 2);
            battle_menu_init_system_function(
                0xD, 0, source->battle_data->misc_unit_id, 0, source->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        }
    }
    if (source->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        battle_action_set_at_list_active();
    } else {
        battle_action_clear_at_list_active();
    }
}

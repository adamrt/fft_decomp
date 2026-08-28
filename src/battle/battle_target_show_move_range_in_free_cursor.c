#include "fft/battle.h"
#include "fft/main_sound.h"
#include "psx/types.h"

void battle_target_show_move_range_in_free_cursor(void) {
    battle_unit_misc_data_t* misc;
    battle_stats_t* stats;
    s32 prev;

    battle_state_start_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_DISPLAY_MOVE_AREA;
    misc = battle_unit_get_casting_misc_data();
    stats = misc->battle_data;
    battle_move_set_reachable_tiles(stats->misc_unit_id, misc->map_x, misc->map_y, misc->map_z);
    battle_target_set_tile_background_color(1, 1);
    prev = g_battle_controller_input;
    g_battle_controller_input = 2;
    g_controller_input_copy_12 = prev;
    main_sound_play_sfx(MAIN_SFX_CONFIRM);
}

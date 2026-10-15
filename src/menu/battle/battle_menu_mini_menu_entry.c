/* Free-cursor move range display and mini-menu openers. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "psx/types.h"

void target_show_move_range_in_free_cursor(void) {
    battle_unit_t* unit;
    unit_stats_t* stats;
    s32 prev;

    camera_enable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_DISPLAY_MOVE_AREA;
    unit = battle_unit_get_casting();
    stats = unit->stats;
    move_set_reachable_tiles(stats->battle_id, unit->map_x, unit->map_y, unit->map_z);
    target_set_tile_background_color(BATTLE_TILE_MARK_SHOW_MOVE, 1);
    prev = g_battle_cursor_input_mode;
    g_battle_cursor_input_mode = BATTLE_CURSOR_INPUT_FREE_CURSOR;
    g_battle_cursor_input_mode_saved = prev;
    main_sound_play_sfx(MAIN_SFX_CONFIRM);
}

void battle_menu_open_free_cursor_mini_menu(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MINI_MENU;
    menu_start_mini_menu_display_thread();
}

/* Leave the view of a unit picked from the AT list: enter the mini-menu state,
 * remove the tile marks and release OPTION.OUT's AT list thread, which waits on
 * g_battle_menu_at_list_selection_finished. */
void battle_menu_open_mini_menu(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MINI_MENU;
    target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 3);
    menu_finish_at_list_selection();
}

void battle_menu_open_mini_menu_help(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MINI_MENU_HELP;
    g_battle_menu_help_opening = 1;
    menu_start_building_thread();
}

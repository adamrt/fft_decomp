#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"

/* Leave the view of a unit picked from the AT list: enter the mini-menu state,
 * remove the tile marks and release OPTION.OUT's AT list thread, which waits on
 * g_battle_menu_at_list_selection_finished. */
void battle_menu_open_mini_menu(void) {
    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MINI_MENU;
    target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 3);
    menu_finish_at_list_selection();
}

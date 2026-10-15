#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_target.h"
#include "psx/pad.h"

/* AbilityPreviewHelp game-state handler: when the help window has gone
 * (battle_menu_is_still_building returns something other than 2), return to the
 * ability preview with map panning. */
void battle_state_handle_ability_preview_help_state(void) {
    if (menu_get_thread_activity_variant() != 2) {
        g_battle_menu_help_opening = 0;
        camera_enable_map_pan();
        g_battle_game_state = BATTLE_GAME_STATE_ABILITY_PREVIEW_HANDLING;
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

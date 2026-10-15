#include "fft/battle.h"
#include "fft/battle_target.h"
#include "psx/pad.h"

/* DisplayMoveArea game-state handler, a unit's move range shown from the free
 * cursor: cross clears the marks, restores the cursor input mode and returns to
 * the free cursor. */
void battle_state_handle_display_move_area_state(void) {
    if (g_main_input_pressed_buttons & PSX_PAD_CROSS) {
        target_set_tile_background_color(BATTLE_TILE_MARK_NONE, 1);
        g_battle_cursor_input_mode = g_battle_cursor_input_mode_saved;
        main_sound_play_sfx(MAIN_SFX_CANCEL);
        battle_state_set_free_cursor();
    }
    camera_handle_rotation_input();
    camera_update_zoom_input();
    camera_update_tilt_input();
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

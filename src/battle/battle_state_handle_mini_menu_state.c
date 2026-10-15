#include "fft/battle.h"
#include "psx/pad.h"

/* MiniMenu game-state handler. Select opens the help window. A menu result of 0
 * (closed) restores the cursor movement option and returns to the free cursor; a
 * result of 0x64 or more shows the unit of AT-list entry result - 0x64. */
void battle_state_handle_mini_menu_state(void) {
    s32 menu_result;

    menu_result = menu_get_thread_activity_variant();
    if (g_main_input_pressed_buttons & PSX_PAD_SELECT) {
        battle_menu_open_mini_menu_help();
    } else if (menu_result == 0) {
        g_battle_cursor_input_mode = g_main_game_options.fields.cursor_movement;
        battle_state_set_free_cursor();
    } else if (menu_result >= 0x64) {
        action_get_next_acting_unit(menu_result - 0x64);
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
}

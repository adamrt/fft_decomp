#include "fft/battle.h"
#include "psx/pad.h"

/* ActiveTurn game-state handler, the view of a unit picked from the AT list:
 * circle or cross returns to the mini menu. */
void battle_state_handle_active_turn_state(void) {
    s32 controller_input;

    controller_input = g_main_input_pressed_buttons;
    if ((controller_input & PSX_PAD_CIRCLE) || (controller_input & PSX_PAD_CROSS)) {
        battle_menu_open_mini_menu();
    }
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
}

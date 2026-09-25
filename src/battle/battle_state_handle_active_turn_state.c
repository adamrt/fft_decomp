#include "fft/battle.h"
#include "psx/pad.h"

void battle_state_handle_active_turn_state(void) {
    s32 controller_input;

    controller_input = g_controller_input_pressed;
    if ((controller_input & PSX_PAD_CIRCLE) || (controller_input & PSX_PAD_CROSS)) {
        battle_menu_open_mini_menu();
    }
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
}

#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"
#include "psx/pad.h"

void battle_state_handle_mini_menu_state(void) {
    s32 menu_result;

    menu_result = battle_menu_is_still_building();
    if (g_controller_input_pressed & PSX_PAD_SELECT) {
        battle_menu_open_mini_menu_help();
    } else if (menu_result == 0) {
        g_battle_controller_input = g_main_game_options.fields.cursor_movement;
        battle_state_set_free_cursor();
    } else if (menu_result >= 0x64) {
        battle_action_get_next_acting_unit(menu_result - 0x64);
    }
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
}

#include "fft/battle.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_state_handle_target_display_state(void) {
    s32 flags;
    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    flags = g_controller_input_pressed;
    if ((flags & PSX_PAD_CIRCLE) || (flags & PSX_PAD_CROSS)) {
        battle_target_move_cursor_to_unit(battle_unit_get_casting_misc_data());
        battle_menu_set_next_script_action_menus();
    }
}

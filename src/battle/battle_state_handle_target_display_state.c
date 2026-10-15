#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/script.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* TargetDisplay game-state handler: circle or cross puts the cursor back on the
 * casting unit and opens the next action menus. */
void battle_state_handle_target_display_state(void) {
    s32 flags;
    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    flags = g_main_input_pressed_buttons;
    if ((flags & PSX_PAD_CIRCLE) || (flags & PSX_PAD_CROSS)) {
        target_move_cursor_to_unit(battle_unit_get_casting());
        battle_menu_set_next_script_action_menus();
    }
}

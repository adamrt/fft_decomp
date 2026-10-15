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

/* StatusExecute game-state handler: once the message has closed (command 7, 8 or
 * 0xff), no animation is pending and the source unit's numeric display is idle,
 * refresh the unit's display and run the between-turn events. */
void battle_state_handle_status_execute_state(void) {
    s32 id;
    battle_unit_t* unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    id = menu_get_selected_command_address()->kind;
    if ((id >= 7) && ((id < 9) || (id == 0xFF))) {
        g_action_post_action = 1;
    }
    if (g_action_post_action != 0) {
        if (g_battle_state_animation_continue_check == 0) {
            unit = battle_unit_get_source();
            if (unit->numeric_display.active == 0) {
                battle_unit_update_display_by_runtime_id(unit->runtime_id);
                action_check_between_turn_events();
            }
        }
    }
}

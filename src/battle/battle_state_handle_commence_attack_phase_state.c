#include "fft/battle.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"

void battle_state_handle_commence_attack_phase_state(void) {
    battle_unit_misc_data_t* unit;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    unit = battle_unit_get_source_misc_data();
    if (unit->attack_phase_state == 3) {
        if (unit->animation_countdown == 0) {
            battle_action_set_only_action_taken(unit->battle_data->misc_unit_id);
            battle_unit_update_post_command_animation_display(unit);
        }
    } else {
        g_battle_action_phase = 0;
        battle_state_announce_next_charged_action();
    }
}

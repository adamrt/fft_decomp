#include "fft/battle.h"

void battle_action_show_caster_post_effect_messages(void) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_casting_misc_data();
    battle_action_queue_post_effect_messages_for_unit(unit);
    if (g_main_game_options.fields.effect_messages != GAME_OPTION_ON) {
        g_battle_action_post_effect_msg_counter = 0;
    }
    if (battle_action_resume_attack_phase_control() == 0) {
        battle_menu_init_system_function(0xA, 0, unit->battle_data->misc_unit_id, 0, 0);
    }
}

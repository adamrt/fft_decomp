#include "fft/battle_action.h"
#include "fft/battle_menu.h"

/* Queue the casting unit's own post-effect messages and show the first one. With
 * effect messages turned off, or nothing to report, an effect message with text
 * id 0 is requested instead. */
void action_show_caster_post_effect_messages(void) {
    battle_unit_t* unit;

    unit = battle_unit_get_casting();
    action_queue_post_effect_messages_for_unit(unit);
    if (g_main_game_options.fields.effect_messages != GAME_OPTION_ON) {
        g_action_post_effect_msg_counter = 0;
    }
    if (action_resume_attack_phase_control() == 0) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
}

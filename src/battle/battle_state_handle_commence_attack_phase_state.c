#include "fft/battle.h"

/* CommenceAttackPhase game-state handler. A Jump waits for the source unit's
 * animation to end, then marks its action taken and enters the after-command
 * state. Any other action starts the pre-action phase and announces the next
 * action that is due. */
void battle_state_handle_commence_attack_phase_state(void) {
    battle_unit_t* unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    unit = battle_unit_get_source();
    if (unit->attack_phase_state == BATTLE_ATTACK_PREPARATION_JUMP) {
        if (unit->animation.wait == 0) {
            action_set_only_action_taken(unit->stats->battle_id);
            battle_unit_update_post_command_animation_display(unit);
        }
    } else {
        g_action_phase = ACTION_PHASE_PRE_ACTION;
        battle_state_announce_next_charged_action();
    }
}

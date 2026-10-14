#include "fft/battle.h"
#include "psx/types.h"

/* Announce the next unit whose charged or performing action comes due.
 *
 * Each of the three action phases is polled in turn; a phase that produces a
 * unit id publishes the announcement state, resolves the spell quote and stops
 * the scan.  Returns 1 once an announcement was set up, 0 when all three phases
 * had nothing pending. */
s32 battle_state_announce_next_charged_action(void) {
    action_command_t command;
    battle_unit_t* unit;
    battle_unit_t* actor;
    s32 result;
    s32 ability_id;
    unit_stats_t* actor_stats;

    unit = battle_unit_get_source();
    do {
        switch (g_action_phase) {
        case ACTION_PHASE_PRE_ACTION:
            result = battle_reaction_prepare_hamedo_for_pending_action(unit->stats->battle_id, &command.ability_id);
            if (result != -1) {
                g_battle_game_state = BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT;
                actor = battle_unit_get_by_battle_id(result & 0xffff);
                g_battle_casting_runtime_id = actor->runtime_id;
                command.skillset_id = 0;
                actor_stats = actor->stats;
                text_select_ability_announcement(&command, actor_stats->battle_id, 1);
                ability_id = command.ability_id;
                result = 0;
                actor->continue_attack_count = 0;
                g_action_post_action = 0;
                actor->sp2_ability_id = ability_id;
                actor->used_ability_id = ability_id;
            }
            break;
        case ACTION_PHASE_ACTING_UNIT:
            result = action_init_current_data(unit->stats->battle_id);
            if (result == 0) {
                g_battle_game_state = BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT;
                g_battle_casting_runtime_id = unit->runtime_id;
                text_select_ability_announcement(&unit->stats->action_command, unit->stats->battle_id, 1);
                ability_id = unit->command_state.ai.data.action.ability_id;
                unit->continue_attack_count = 0;
                g_action_post_action = 0;
                unit->sp2_ability_id = ability_id;
                unit->used_ability_id = ability_id;
            } else {
                if (result >= 0) {
                    if (result < 4) {
                        battle_gfx_activate_numerical_sprite_data(unit, result);
                    }
                }
                battle_unit_set_animation_based_on_status(unit);
                battle_unit_update_post_command_animation_display(unit);
                result = -1;
            }
            break;
        case ACTION_PHASE_REACTING_UNIT:
            result = battle_reaction_prepare_next(&command.ability_id);
            if (result != -1) {
                g_battle_game_state = BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT;
                actor = battle_unit_get_by_battle_id(result & 0xffff);
                g_battle_casting_runtime_id = actor->runtime_id;
                command.skillset_id = 0;
                actor_stats = actor->stats;
                text_select_ability_announcement(&command, actor_stats->battle_id, 1);
                ability_id = command.ability_id;
                result = 0;
                actor->continue_attack_count = 0;
                g_action_post_action = 0;
                actor->sp2_ability_id = ability_id;
                actor->used_ability_id = ability_id;
            }
            break;
        }
        if (result != -1) {
            return 1;
        }
        g_action_phase = g_action_phase + 1;
    } while (g_action_phase < ACTION_PHASE_COUNT);
    return 0;
}

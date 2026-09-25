#include "fft/battle.h"

/*
 * Insert the best action into the ranked list, then reset it.
 *
 * The candidate must beat the movement baseline and the last ranked entry.
 * Insert through the shift loop's final index to preserve register lifetimes.
 */
void battle_ai_insert_ranked_action(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 i;
    s32 j;

    if (battle_ai_compare_target_priority_and_hit_rate((battle_ai_action_rank_t*)&ai->best_action.rank_byte,
            (battle_ai_action_rank_t*)&ai->inverted_priority_action.rank_byte)
        == BATTLE_AI_RANK_ABOVE) {
        if (battle_ai_is_action_higher_ranked(&ai->best_action, &ai->ranked_actions[7])) {
            for (i = 0; i < 8; i++) {
                if (battle_ai_is_action_higher_ranked(&ai->best_action, &ai->ranked_actions[i]))
                    break;
            }
            if (i != 8) {
                for (j = 7; i < j; j--)
                    battle_ai_transfer_halfword_values((u16*)&ai->ranked_actions[j], (u16*)&ai->ranked_actions[j - 1],
                        sizeof(battle_ai_action_data_t));
                battle_ai_transfer_halfword_values(
                    (u16*)&ai->ranked_actions[j], (u16*)&ai->best_action, sizeof(battle_ai_action_data_t));
            }
        }
    }
    battle_ai_reset_action_ranking_fields(&ai->best_action);
}

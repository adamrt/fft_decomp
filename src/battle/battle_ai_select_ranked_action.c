#include "fft/battle_ai.h"

/*
 * Select an action by testing ranked entries in order.
 *
 * Each visited entry consumes a random draw, including zero-hit entries.
 * If every draw fails but any hit rate is nonzero, select the first entry.
 */
s32 battle_ai_select_ranked_action(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 i;
    s32 any;
    s32 chance;

    any = 0;
    for (i = 0; i < 8; i++) {
        chance = ai->ranked_actions[i].base_hit_percent;
        if (chance != 0) {
            any = 1;
        }
        if ((rand() % 100) < chance) {
            battle_ai_transfer_halfword_values(
                (u16*)&ai->selected_action, (u16*)&ai->ranked_actions[i], sizeof(battle_ai_action_data_t));
            return 1;
        }
    }
    if (any == 0) {
        return 0;
    }
    battle_ai_transfer_halfword_values(
        (u16*)&ai->selected_action, (u16*)&ai->ranked_actions[0], sizeof(battle_ai_action_data_t));
    return 1;
}

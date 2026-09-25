#include "fft/battle.h"

/*
 * Reset an action's ranking fields and selection defaults.
 *
 * The remaining payload is retained; this is not a whole-record clear.
 */
void battle_ai_reset_action_ranking_fields(battle_ai_action_data_t* action) {
    /* Signed view preserves addiu -32767; direct u16 assignment emits ori. */
    *(s16*)&action->priority = -0x7fff;
    action->rank_byte = 0;
    action->base_hit_percent = 0;
    action->wait_facing_hint = BATTLE_AI_DIRECTION_CHOOSE_AT_WAIT;
    action->reflected_action = 0;
}

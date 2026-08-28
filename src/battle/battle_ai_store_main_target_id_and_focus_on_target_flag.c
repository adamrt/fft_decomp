#include "fft/battle_ai.h"

/*
 * Focus the acting unit's decision on one target and its coordinates.
 *
 * Set focus flag 0x40 and clear stay-near flag 0x20, retaining other flags.
 */
void battle_ai_store_main_target_id_and_focus_on_target_flag(s32 unit_id) {
    battle_ai_unit_decision_t* decision = g_battle_ai_data_base.acting_unit_decision;
    u8 flags = decision->flags;

    decision->main_target_id = unit_id;
    decision->flags = (flags | BATTLE_AI_DECISION_FOCUS_TARGET) & ~BATTLE_AI_DECISION_STAY_NEAR_COORDINATES;
    battle_ai_transfer_unit_coordinates(unit_id, &decision->target);
}

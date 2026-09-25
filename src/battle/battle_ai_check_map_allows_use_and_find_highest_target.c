#include "fft/battle.h"
#include "psx/types.h"

s32 battle_ai_check_map_allows_use_and_find_highest_target(void) {
    battle_ai_data_t* ai;

    ai = &g_battle_ai_data_base;
    if (g_battle_ai_data_base.decision_state != 0) {
        /* Resume: re-enter the step that suspended. */
        if (g_battle_ai_data_base.find_highest_target_state != 0) {
            goto second;
        }
        goto first;
    }
    if (battle_ai_check_ability_use_at_coords(&ai->acting_unit_coords) != 0) {
        g_battle_ai_data_base.current_action.target_flags_set = 1;
    first:
        if (battle_ai_check_set_highest_unit_priority() == -1) {
            ai->find_highest_target_state = 0;
            return -1;
        }
    }
    if (ai->current_action.coords.word == ai->acting_unit_coords.word) {
        return 0;
    }
    if (battle_ai_check_ability_use_at_coords(&ai->current_action.coords) == 0) {
        return 0;
    }
    ai->current_action.target_flags_set = 0;
second:
    if (battle_ai_check_set_highest_unit_priority() != -1) {
        return 0;
    }
    ai->find_highest_target_state = 1;
    return -1;
}

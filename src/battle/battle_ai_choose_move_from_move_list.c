#include "fft/battle_ai.h"
#include "fft/data.h"

s32 battle_ai_choose_move_from_move_list(void) {
    battle_ai_data_t* ai;
    battle_ai_ability_entry_t* entry;

    ai = &g_battle_ai_data_base;
    if (g_battle_ai_data_base.decision_state != 0) {
        goto retry; /* resume the suspended evaluation inside the loop */
    }
    g_battle_ai_data_base.ability_counter = 0;
    while (1) {
        entry = &ai->ability_lists[ai->acting_unit_battle_id][ai->ability_counter];
        if (entry->skillset_flags.bytes.skillset == SKILLSET_ID_NONE) {
            return 0;
        }
        battle_ai_load_ability_entry(entry);
    retry:
        if (battle_ai_evaluate_ability_outcome() == -1) {
            return -1;
        }
        ai->ability_counter += 1;
        if (ai->ability_counter >= 0x22) {
            return 0;
        }
    }
}

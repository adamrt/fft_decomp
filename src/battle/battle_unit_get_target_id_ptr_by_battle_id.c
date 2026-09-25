#include "fft/battle.h"
#include "psx/types.h"

battle_ai_command_action_t* battle_unit_get_target_id_ptr_by_battle_id(u32 battle_id) {
    battle_unit_misc_data_t* unit = battle_unit_get_misc_data_by_battle_id(battle_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return &unit->command_state.ai.data.action;
}

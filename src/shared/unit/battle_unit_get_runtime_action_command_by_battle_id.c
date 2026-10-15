#include "fft/battle_unit.h"
#include "psx/types.h"

/* The command action record (command_state.ai.data.action) of the unit in slot
 * battle_id, or 0 when it has no runtime record. */
action_command_t* battle_unit_get_runtime_action_command_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = battle_unit_get_by_battle_id(battle_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return &unit->command_state.ai.data.action;
}

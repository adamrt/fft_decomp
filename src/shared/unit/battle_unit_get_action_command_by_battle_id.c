#include "fft/battle_unit.h"
#include "psx/types.h"

/* Returns an occupied unit slot's action command, or 0 for an
 * empty slot or an id past the last slot. No retail code calls it. */
action_command_t* battle_unit_get_action_command_by_battle_id(s32 battle_id) {
    if (battle_id < BATTLE_UNIT_SLOT_COUNT) {
        if (g_battle_unit_stats[battle_id].entd_slot != BATTLE_ENTD_SLOT_NONE) {
            return &g_battle_unit_stats[battle_id].action_command;
        }
    }
    return 0;
}

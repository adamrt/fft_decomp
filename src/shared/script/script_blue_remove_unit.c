#include "fft/battle_gfx.h"
#include "fft/battle_script.h"
#include "fft/battle_unit.h"

/* BlueRemoveUnit (event instruction 0x99): tints every non-player unit whose
 * ENTD unit id is unit_id blue (red and green modulated by -0x1f).
 *
 * The team test reads the current team byte (0x05), not the initial one at
 * 0x1ba. */
void script_blue_remove_unit(s32 unit_id) {
    s32 runtime_id;
    s32 battle_index;
    unit_stats_t* unit;
    s32 found_battle_id[2];

    for (runtime_id = 0; runtime_id < EVENT_UNIT_SLOT_COUNT; runtime_id++) {
        if (battle_unit_has_runtime_id(runtime_id) == 0) {
            continue;
        }
        battle_index = battle_unit_get_battle_id_by_runtime_id(runtime_id);
        if (battle_index == -1) {
            continue;
        }
        unit = battle_unit_find_stats_by_entd_unit_id(
            battle_unit_get_stats_from_battle_id(battle_index)->unit_id, found_battle_id);
        if ((unit->team_flags & BATTLE_TEAM_MASK) == BATTLE_TEAM_BLUE) {
            continue;
        }
        if (found_battle_id[0] == -2) {
            continue;
        }
        if (unit->unit_id != unit_id) {
            continue;
        }
        battle_gfx_start_event_unit_fade_out(runtime_id, 2, -0x1F, -0x1F, 0);
    }
}

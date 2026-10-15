#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_ai.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/memory.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Clears the enterable flag on the tile of every dead or jumping unit. */
void move_clear_enterable_flags_under_dead_or_jumping_units(void) {
    s32 unit_id;

    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        unit_stats_t* unit = &g_battle_unit_stats[unit_id];

        if ((unit->entd_slot != BATTLE_ENTD_SLOT_NONE)
            && ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)]
                    & (STATUS_BYTE_MASK(STATUS_ID_DEAD) | STATUS_BYTE_MASK(STATUS_ID_JUMP)))
                != 0)) {
            g_move_frontier_flags_ptr[map_calculate_location(unit)] &= ~FRONTIER_FLAG_ENTERABLE;
        }
    }
}

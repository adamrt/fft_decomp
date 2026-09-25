#include "fft/battle.h"
#include "psx/types.h"

enum {
    /* Tile flag byte in the 0x1f800200 scratch pad: 0x10 reachable. */
    TILE_FLAG_REACHABLE = 0x10,
};

/* Clears the reachable flag on the tile of every dead or jumping unit. */
void battle_move_clear_reachable_flags_under_dead_or_jumping_units(void) {
    s32 unit_id;

    for (unit_id = 0; unit_id < BATTLE_UNIT_SLOT_COUNT; unit_id++) {
        battle_stats_t* unit = &g_battle_unit_stats[unit_id];

        if ((unit->entd_slot != BATTLE_ENTD_SLOT_NONE)
            && ((unit->status_sets.current[0]
                    & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP)))
                != 0)) {
            g_battle_move_frontier_flags_ptr[battle_map_calculate_location(unit)] &= ~TILE_FLAG_REACHABLE;
        }
    }
}

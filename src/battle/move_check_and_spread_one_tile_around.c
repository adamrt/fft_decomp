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

/* Spread one step from the current tile. Source i uses direction i % 4 (east,
 * north, west, south): 0..3 leave from the ground, 4..7 from the top of the
 * unit on the tile, and 8 and up (outer_count 0x24) from that unit's standing
 * height + i / 4 - 2. Each source whose neighbor is on the map tries
 * inner_count destinations: either level, on the ground or on a unit. */
void move_check_and_spread_one_tile_around(void) {
    move_spread_state_t* state;
    s32 i;
    s32 j;

    state = g_move_scratch_pad_ptr;
    move_transfer_tiles_height_halves_and_slope_to_scratch_pad();
    for (i = 0; i < state->outer_count; i++) {
        state->source_unit_record_flag = i >= 4;
        g_move_spread_preset_table[i % 4]();
        move_set_spreading_tile_x_and_y_coordinates();
        if ((move_check_spreading_tile_coordinates() == 0) && (move_init_source_panel(i) == 0)) {
            for (j = 0; j < state->inner_count; j++) {
                move_propagate_destination(j, 0);
            }
        }
    }
}

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

/* Reset the best-candidate fields of the spread state and load the current
 * tile's heights. */
void move_init_spread_scratch(void) {
    move_spread_state_t* spread = g_move_scratch_pad_ptr;
    spread->best_height_delta = 0xFF;
    spread->best_fly_height_delta = 0xFF;
    spread->budget_matches = 0;
    spread->candidate_saved = 0;
    spread->best_extra_span = 0x7F;
    move_transfer_tiles_height_halves_and_slope_to_scratch_pad();
}

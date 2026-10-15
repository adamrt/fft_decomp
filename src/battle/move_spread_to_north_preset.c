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

/* Step +y: the slope shifts select the source's exit side and the
 * destination's entry side. */
void move_spread_to_north_preset(void) {
    move_spread_state_t* spread = g_move_scratch_pad_ptr;
    spread->y_step = 1;
    spread->source_side_shift = 6;
    spread->x_step = 0;
    spread->destination_side_shift = 4;
}

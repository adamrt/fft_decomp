#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

/* Returns whether a unit with no remaining move or action should end its turn. */
s32 action_should_end_unit_turn(const unit_stats_t* unit) {
    s32 cannot_act;
    u8 no_move;
    u8 no_action;
    u8 taken;

    cannot_act = battle_unit_get_action_restriction(unit);
    no_move = unit->movement_taken
        | (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DONT_MOVE)] & STATUS_BYTE_MASK(STATUS_ID_DONT_MOVE));
    no_action = unit->action_taken | cannot_act;
    taken = unit->movement_taken | unit->action_taken;
    if (no_move != 0 && no_action != 0 && taken != 0) {
        return 1;
    }
    return 0;
}

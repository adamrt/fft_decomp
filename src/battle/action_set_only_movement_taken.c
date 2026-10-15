#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_status.h"
#include "psx/types.h"

void action_set_only_movement_taken(s32 unit_id) {
    action_set_move_act_flags(unit_id, 1, 0);
}

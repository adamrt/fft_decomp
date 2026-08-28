#include "fft/battle.h"
#include "psx/types.h"

void battle_action_set_only_action_taken(s32 unit_id) {
    battle_action_set_move_act_flags(unit_id, 0, 1);
}

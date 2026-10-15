#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Validate the action in place with action_prepare_attack phase 0, which
 * consumes nothing and leaves the unit's turn flags alone. */
void action_call_attack_preparation(action_command_t* action) {
    action_prepare_attack(action, action, 0);
}

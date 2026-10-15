#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

void battle_unit_start_post_attack_animation_display(battle_unit_t* attacker, battle_unit_t* target) {
    if (attacker != 0 && target != 0) {
        battle_unit_set_target_animation_from_attack_type(attacker, target);
        battle_gfx_prepare_post_action_display_by_runtime_id(target->runtime_id);
    }
}

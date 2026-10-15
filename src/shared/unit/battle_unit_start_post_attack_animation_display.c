#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* Start the target's reaction to the attacker's action and its post-action
 * display. Both parameters are unit records; byte 4 is the unit id. */
void battle_unit_start_post_attack_animation_display(battle_unit_t* attacker, battle_unit_t* target) {
    if (attacker != 0 && target != 0) {
        battle_unit_set_target_animation_from_attack_type(attacker, target);
        battle_gfx_prepare_post_action_display_by_runtime_id(target->runtime_id);
    }
}

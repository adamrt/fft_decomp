#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "psx/types.h"

/* Aim a projectile from one unit at another: set up the direct trajectory to the
 * target's tile and make the target the attacker's only target. Returns 1 when
 * both units exist. Nothing in the game calls it. */
s32 effect_init_single_target_trajectory(u32 attacker_id, u32 target_id) {
    battle_unit_t* attacker;
    battle_unit_t* target;
    SVECTOR target_position;

    attacker = battle_unit_get_by_battle_id(attacker_id);
    target = battle_unit_get_by_battle_id(target_id);
    if (target != 0) {
        main_util_set_svector(&target_position, target->map_x, target->map_y, target->map_z);
        effect_check_direct_trajectory_to_target(attacker_id, &target_position, target_id);
        if (attacker != 0) {
            attacker->strike.target_count = 1;
            attacker->strike.target_list[0] = target_id;
            return 1;
        }
    }
    return 0;
}

#include "fft/battle.h"

s32 battle_effect_init_single_target_trajectory(u32 attacker_id, u32 target_id) {
    battle_unit_misc_data_t* attacker;
    battle_unit_misc_data_t* target;
    SVECTOR target_position;

    attacker = battle_unit_get_misc_data_by_battle_id(attacker_id & 0xFFFF);
    target = battle_unit_get_misc_data_by_battle_id(target_id & 0xFFFF);
    if (target != 0) {
        main_util_set_svector(&target_position, target->map_x, target->map_y, target->map_z);
        battle_effect_check_direct_trajectory_to_target(attacker_id, &target_position, target_id);
        if (attacker != 0) {
            attacker->target_count = 1;
            attacker->target_list[0] = target_id;
            return 1;
        }
    }
    return 0;
}

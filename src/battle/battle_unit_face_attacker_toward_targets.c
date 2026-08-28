#include "fft/battle.h"
#include "psx/types.h"

/* For each live target in the attacker's target list, tell it to face the
   attacker. */
void battle_unit_face_attacker_toward_targets(battle_unit_misc_data_t* unit) {
    s32 i;
    battle_unit_misc_data_t* target;

    if (unit->target_count != 0) {
        i = 0;
        while (i < (s32)unit->target_count) {
            target = battle_unit_get_misc_data_by_battle_id(unit->target_list[i]);
            if (target != 0) {
                battle_unit_set_rider_mount_x_y_facing(target);
            }
            i++;
        }
    }
}

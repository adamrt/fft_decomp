#include "fft/battle.h"

void battle_unit_set_animation_based_on_mounted_state(battle_unit_misc_data_t* unit) {
    s32 anim;
    s32 s1v;
    s32 fv;
    battle_unit_misc_data_t* other;

    anim = 0x32;
    if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_RIDER) {
        fv = battle_move_validate_float_fly(unit) & 0xFF;
        if (fv < 0) {
            s1v = 9;
        } else if (fv < 2) {
            s1v = 2;
            if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_MOUNT) {
                other = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
                if (other != 0) {
                    battle_unit_store_animation_facing(0x32, (s16)unit->facing, other);
                }
            }
        } else {
            s1v = 9;
        }
        if (s1v == 0) {
            return;
        }
        anim = s1v;
    }
    battle_unit_store_animation_facing(anim, (s16)unit->facing, unit);
}

#include "fft/battle.h"
#include "psx/types.h"

/*
 * Selects the movement idle animation from walk speed, height mode and mount state.
 *
 * The height-mode result is deliberately truncated to its low byte, and
 * facing is read signed (`lh`) throughout.
 */
void battle_unit_set_idle_animation_for_movement(battle_unit_misc_data_t* unit) {
    s32 anim;
    s32 movement_type;
    s32 walk_speed;
    battle_unit_misc_data_t* mount;

    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_RIDER) {
        battle_unit_store_animation_facing(0x32, *(s16*)&unit->facing, unit);
        return;
    }
    movement_type = (u8)battle_move_validate_float_fly(unit);
    /* A combined 0..1 range test folds to one unsigned compare; the target
     * tests the sign first, so that path jumps into the mounted arm. */
    if (movement_type < 0) {
        goto mounted;
    }
    if (movement_type < 2) {
        walk_speed = unit->walk_speed.word;
        if (walk_speed >= 0x3000) {
            anim = 0xD;
        } else {
            anim = 0xC;
            if (walk_speed < 0x1401) {
                anim = 0xE;
            }
        }
        if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_MOUNT) {
            mount = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
            if (mount != 0) {
                battle_unit_store_animation_facing(0x32, *(s16*)&unit->facing, mount);
            }
        }
    } else {
    mounted:
        walk_speed = unit->walk_speed.word;
        if (walk_speed >= 0x3000) {
            anim = 0xA;
        } else {
            anim = 0x9;
            if (walk_speed < 0x1401) {
                anim = 0xB;
            }
        }
    }
    if (anim != 0) {
        battle_unit_store_animation_facing(anim, *(s16*)&unit->facing, unit);
    }
}

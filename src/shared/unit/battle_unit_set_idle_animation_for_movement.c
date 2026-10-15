#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/*
 * Select the animation of a unit that is walking: riding for a rider, else
 * the walk of its walk speed (fast from 0x3000, slow up to 0x1400), or the
 * submerged idle of that speed in water two or more deep. A mount on land
 * also puts its rider in the riding animation.
 *
 * The height-mode result is deliberately truncated to its low byte, and
 * facing is read signed (`lh`) throughout.
 */
void battle_unit_set_idle_animation_for_movement(battle_unit_t* unit) {
    s32 anim;
    s32 walk_speed;
    battle_unit_t* mount;

    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_RIDING, unit->facing, unit);
        return;
    }
    switch (move_get_water_depth(unit)) {
    case 0:
    case 1:
        walk_speed = unit->walk_speed;
        if (walk_speed >= 0x3000) {
            anim = BATTLE_UNIT_ANIMATION_WALK_FAST;
        } else {
            anim = BATTLE_UNIT_ANIMATION_WALK;
            if (walk_speed < 0x1401) {
                anim = BATTLE_UNIT_ANIMATION_WALK_SLOW;
            }
        }
        if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
            mount = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
            if (mount != 0) {
                battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_RIDING, unit->facing, mount);
            }
        }
        break;
    case 2:
    default:
        walk_speed = unit->walk_speed;
        if (walk_speed >= 0x3000) {
            anim = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_HASTE;
        } else {
            anim = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
            if (walk_speed < 0x1401) {
                anim = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE_SLOW;
            }
        }
        break;
    }
    if (anim != 0) {
        battle_unit_store_animation_facing(anim, unit->facing, unit);
    }
}

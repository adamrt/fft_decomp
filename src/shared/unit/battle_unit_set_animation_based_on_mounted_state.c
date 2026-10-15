#include "fft/battle_move.h"
#include "fft/battle_unit.h"

/* Give a rider the riding animation and any other unit the standing one, or
 * the submerged idle in water two or more deep; a mount on land also puts
 * its rider back in the riding animation. The depth is a byte, so its
 * negative test never holds. */
void battle_unit_set_animation_based_on_mounted_state(battle_unit_t* unit) {
    s32 anim;
    s32 selected;
    s32 water_depth;
    battle_unit_t* other;

    anim = BATTLE_UNIT_ANIMATION_RIDING;
    if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_RIDER) {
        water_depth = move_get_water_depth(unit) & 0xFF;
        if (water_depth < 0) {
            selected = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
        } else if (water_depth < 2) {
            selected = BATTLE_UNIT_ANIMATION_STAND;
            if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
                other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
                if (other != 0) {
                    battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_RIDING, unit->facing, other);
                }
            }
        } else {
            selected = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
        }
        if (selected == 0) {
            return;
        }
        anim = selected;
    }
    battle_unit_store_animation_facing(anim, unit->facing, unit);
}

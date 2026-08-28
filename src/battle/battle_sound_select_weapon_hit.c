#include "fft/main_unit.h"
#include "psx/types.h"

/* Per-weapon-type sound tables, 0x20 entries each: swing, hit, and the
 * guarded/nullified reaction sound. */

/* Play the weapon swing sound, or the hit sound for each current target.
 *
 * The weapon type comes from the unit's used weapon; Throwing and later types use
 * type 1. In hit mode each target's action miss type selects the table, and a
 * critical hit plays sound 0x72. */
void battle_sound_select_weapon_hit(battle_unit_misc_data_t* unit, s32 hit) {
    battle_unit_misc_data_t* target;
    s32 weapon_type;
    s32 i;

    weapon_type = main_item_get_data_pointer(unit->used_weapon_id)->type;
    if (weapon_type >= ITEM_TYPE_THROWING) {
        weapon_type = ITEM_TYPE_KNIFE;
    }
    switch (hit) {
    case 0:
        battle_sound_play_movement_sfx(unit, g_battle_sound_weapon_miss_sfx_ids[weapon_type]);
        break;
    case 1:
        for (i = 0; i < unit->target_count; i++) {
            target = battle_unit_get_misc_data_by_battle_id(unit->target_list[i]);
            if (target == 0 || target->battle_data == 0) {
                continue;
            }
            switch (target->battle_data->action.miss_type) {
            case BATTLE_ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case BATTLE_ACTION_MISS_TYPE_LEFT_HAND_EVADE:
            case BATTLE_ACTION_MISS_TYPE_GOLEM_GUARD:
                battle_sound_play_movement_sfx(unit, g_battle_sound_shield_deflect_sfx_ids[weapon_type]);
                break;
            case BATTLE_ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case BATTLE_ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
            case BATTLE_ACTION_MISS_TYPE_FORCED_FAILURE:
            case BATTLE_ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case BATTLE_ACTION_MISS_TYPE_CATCH:
                battle_sound_play_movement_sfx(unit, 0x30);
                break;
            case BATTLE_ACTION_MISS_TYPE_NULLIFIED:
            case BATTLE_ACTION_MISS_TYPE_ACCURACY_MISS:
            case BATTLE_ACTION_MISS_TYPE_CANCELLED:
                break;
            default:
                if (target->battle_data->action.critical != 0) {
                    battle_sound_play_movement_sfx(unit, 0x72);
                } else {
                    battle_sound_play_movement_sfx(unit, g_battle_sound_weapon_hit_sfx_ids[weapon_type]);
                }
                break;
            }
        }
        break;
    }
}

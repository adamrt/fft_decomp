#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/sound.h"
#include "psx/types.h"

/* Per-weapon-type sound tables, 0x20 entries each: swing, hit, and the
 * guarded/nullified reaction sound. */

/* Play the weapon swing sound, or the hit sound for each current target.
 *
 * The weapon type comes from the unit's used weapon; Throwing and later types use
 * type 1. In hit mode each target's action miss type selects the table, and a
 * critical hit plays sound 0x72. */
void sound_play_weapon_sfx(battle_unit_t* unit, s32 hit) {
    battle_unit_t* target;
    s32 weapon_type;
    s32 i;

    weapon_type = main_item_get_data_pointer(unit->strike.used_weapon_id)->type;
    if (weapon_type >= ITEM_TYPE_SHURIKEN) {
        weapon_type = ITEM_TYPE_KNIFE;
    }
    switch (hit) {
    case 0:
        sound_play_movement_sfx(unit, g_sound_weapon_swing_sfx_ids[weapon_type]);
        break;
    case 1:
        for (i = 0; i < unit->strike.target_count; i++) {
            target = battle_unit_get_by_battle_id(unit->strike.target_list[i]);
            if (target == 0 || target->stats == 0) {
                continue;
            }
            switch (target->stats->action.miss_type) {
            case ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case ACTION_MISS_TYPE_LEFT_HAND_EVADE:
            case ACTION_MISS_TYPE_GOLEM_GUARD:
                sound_play_movement_sfx(unit, g_sound_shield_deflect_sfx_ids[weapon_type]);
                break;
            case ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
            case ACTION_MISS_TYPE_FORCED_FAILURE:
            case ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case ACTION_MISS_TYPE_CATCH:
                sound_play_movement_sfx(unit, MAIN_SFX_ERROR_MESSAGE);
                break;
            case ACTION_MISS_TYPE_NULLIFIED:
            case ACTION_MISS_TYPE_ACCURACY_MISS:
            case ACTION_MISS_TYPE_CANCELED:
                break;
            default:
                if (target->stats->action.critical != 0) {
                    sound_play_movement_sfx(unit, MAIN_SFX_CRITICAL_HIT);
                } else {
                    sound_play_movement_sfx(unit, g_sound_weapon_hit_sfx_ids[weapon_type]);
                }
                break;
            }
        }
        break;
    }
}

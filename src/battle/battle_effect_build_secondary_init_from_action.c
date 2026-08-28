#include "fft/battle.h"
#include "psx/types.h"

/* Fill a secondary-effect init record with the unit's current action targets.
 *
 * Each target block records the target's unit id and a result animation
 * derived from its action data. With no targets the first block instead holds
 * the cursor tile position. The record's target blocks form an array of up to
 * 16 entries starting at the header's first `target` block. */
void battle_effect_build_secondary_init_from_action(
    battle_unit_misc_data_t* unit, battle_effect_secondary_init_t* init) {
    battle_unit_misc_data_t* target;
    battle_action_data_t* action;
    s32 i;

    init->target_count = unit->target_count;
    init->caster.fields.target_type = 0;
    init->caster.fields.caster_id = unit->unit_id;
    init->used_weapon_id = unit->used_weapon_id;
    if (init->target_count != 0) {
        for (i = 0; i < init->target_count; i++) {
            target = battle_unit_get_misc_data_by_battle_id(unit->target_list[i]);
            (&init->target)[i].fields.target_type = 0;
            (&init->target)[i].fields.target_id = target->unit_id;
            action = &target->battle_data->action;
            switch (action->miss_type) {
            case BATTLE_ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case BATTLE_ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case BATTLE_ACTION_MISS_TYPE_LEFT_HAND_EVADE:
            case BATTLE_ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
                (&init->target)[i].fields.result_animation = 3;
                break;
            case BATTLE_ACTION_MISS_TYPE_NULLIFIED:
            case BATTLE_ACTION_MISS_TYPE_ACCURACY_MISS:
            case BATTLE_ACTION_MISS_TYPE_FORCED_FAILURE:
            case BATTLE_ACTION_MISS_TYPE_CANCELLED:
            case BATTLE_ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case BATTLE_ACTION_MISS_TYPE_CATCH:
                (&init->target)[i].fields.result_animation = 4;
                break;
            case BATTLE_ACTION_MISS_TYPE_GOLEM_GUARD:
                (&init->target)[i].fields.result_animation = 5;
                break;
            default:
                if (action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT) {
                    (&init->target)[i].fields.result_animation = 6;
                } else if (action->critical != 0) {
                    (&init->target)[i].fields.result_animation = 1;
                } else {
                    (&init->target)[i].fields.result_animation = 0;
                }
                break;
            }
        }
    } else {
        init->target.block.values[0] = 1;
        init->target.block.values[1] = g_battle_cursor_x;
        init->target.block.values[2] = g_battle_cursor_z;
        init->target.block.values[3] = g_battle_cursor_y;
    }
}

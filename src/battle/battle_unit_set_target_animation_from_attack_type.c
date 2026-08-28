#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/battle_gfx.h"
#include "fft/battle_move.h"

/*
 * Pick and start the reaction animation a target plays after an action
 * resolves, then apply the action's special-effect side effects.
 */
void battle_unit_set_target_animation_from_attack_type(
    battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target) {
    battle_stats_t* stats;
    battle_action_data_t* action;
    s32 animation;
    s32 delta;
    s32 attacker_height;
    s32 target_height;
    u32 float_fly;

    if (attacker == target) {
        return;
    }
    if ((target->encoded_animation >> 1) == 0x1a) {
        return;
    }
    if ((target->encoded_animation >> 1) == 0x34) {
        return;
    }
    float_fly = (u8)battle_move_validate_float_fly(target);
    animation = 9;
    if (float_fly < 2) {
        stats = target->battle_data;
        if (stats != 0) {
            action = &stats->action;
            animation = 2;
            switch (action->miss_type) {
            case BATTLE_ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case BATTLE_ACTION_MISS_TYPE_LEFT_HAND_EVADE:
                battle_unit_face_towards_action_target(target, attacker);
                battle_gfx_set_thrown_item_graphic_palette(action->item_lost, target);
                attacker_height = (s32)battle_gfx_get_unit_spritesheet_height_by_misc_id(attacker->unit_id) / 2
                    - (u16)attacker->screen.vy;
                target_height = (s32)battle_gfx_get_unit_spritesheet_height_by_misc_id(target->unit_id) / 2
                    - (u16)target->screen.vy;
                delta = (s16)attacker_height - (s16)target_height;
                if (delta < -0xb) {
                    animation = 0x5a;
                } else {
                    animation = 0x59;
                    if (delta >= 0xc) {
                        animation = 0x58;
                    }
                }
                break;
            case 1:
            case BATTLE_ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
            case BATTLE_ACTION_MISS_TYPE_NULLIFIED:
            case BATTLE_ACTION_MISS_TYPE_ACCURACY_MISS:
            case BATTLE_ACTION_MISS_TYPE_FORCED_FAILURE:
                if (action->status_change & 0x80) {
                    battle_unit_face_towards_action_target(target, attacker);
                    animation = 0x18;
                }
                break;
            case BATTLE_ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case BATTLE_ACTION_MISS_TYPE_CATCH:
                battle_unit_face_towards_action_target(target, attacker);
                animation = 0x1b;
                break;
            case BATTLE_ACTION_MISS_TYPE_CANCELLED:
            case BATTLE_ACTION_MISS_TYPE_REFLECTED:
            case BATTLE_ACTION_MISS_TYPE_GOLEM_GUARD:
                break;
            default:
                if (action->hit != 0) {
                    if (((s16)action->hp_damage != 0) || ((s16)action->mp_damage != 0)) {
                        animation = 0x19;
                    } else if (((s16)action->hp_healing != 0) || ((s16)action->mp_healing != 0)) {
                        animation = 0x1b;
                    }
                }
                break;
            }

            if (animation != 0) {
                if (target->spritesheet_id < 0x9b) {
                    battle_unit_store_animation_facing_movement_data(animation, (s16)target->facing, target);
                } else if (animation == 0x19) {
                    battle_unit_store_animation_facing_movement_data(0x19, (s16)target->facing, target);
                }
            }
            if (action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_KNOCKBACK) {
                battle_move_transfer_target_coordinates(attacker, target);
            }
            if (action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_POACH) {
                target->status_flags_5_6 |= BATTLE_MISC_STATUS_POACHED;
            }
            if (action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_MORBOL) {
                target->status_flags_5_6 |= BATTLE_MISC_STATUS_MORBOL;
            }
            if (action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT) {
                battle_ai_init_unit_abilities(target->battle_data->misc_unit_id);
            }
            if (action->special_effect & BATTLE_ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                battle_ai_init_unit_abilities(target->battle_data->misc_unit_id);
            }
            return;
        }
        animation = 0x19;
    }
    battle_unit_store_animation_facing(animation, (s16)target->facing, target);
}

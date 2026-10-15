#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"

/*
 * Pick and start the reaction animation a target plays after an action
 * resolves, then apply the action's special-effect side effects.
 */
void battle_unit_set_target_animation_from_attack_type(battle_unit_t* attacker, battle_unit_t* target) {
    unit_stats_t* stats;
    action_data_t* action;
    s32 animation;
    s32 delta;
    s16 attacker_height;
    s16 target_height;
    u32 water_depth;

    if (attacker == target) {
        return;
    }
    if ((target->animation.sequence_id >> 1) == BATTLE_UNIT_ANIMATION_DEAD) {
        return;
    }
    if ((target->animation.sequence_id >> 1) == BATTLE_UNIT_ANIMATION_DEATH) {
        return;
    }
    water_depth = move_get_water_depth(target);
    animation = BATTLE_UNIT_ANIMATION_SUBMERGED_IDLE;
    if (water_depth < 2) {
        stats = target->stats;
        if (stats != 0) {
            action = &stats->action;
            animation = BATTLE_UNIT_ANIMATION_STAND;
            switch (action->miss_type) {
            case ACTION_MISS_TYPE_RIGHT_HAND_EVADE:
            case ACTION_MISS_TYPE_LEFT_HAND_EVADE:
                battle_unit_face_towards_action_target(target, attacker);
                battle_gfx_set_thrown_item_graphic_palette(action->item_lost, target);
                attacker_height = battle_gfx_get_unit_spritesheet_height_by_runtime_id(attacker->runtime_id) / 2
                    - attacker->world_coords.vy;
                target_height = battle_gfx_get_unit_spritesheet_height_by_runtime_id(target->runtime_id) / 2
                    - target->world_coords.vy;
                delta = attacker_height - target_height;
                if (delta < -0xb) {
                    animation = BATTLE_UNIT_ANIMATION_WEAPON_GUARD_LOW;
                } else {
                    animation = BATTLE_UNIT_ANIMATION_WEAPON_GUARD_LEVEL;
                    if (delta >= 0xc) {
                        animation = BATTLE_UNIT_ANIMATION_WEAPON_GUARD_HIGH;
                    }
                }
                break;
            case ACTION_MISS_TYPE_ACCESSORY_EVADE:
            case ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD:
            case ACTION_MISS_TYPE_NULLIFIED:
            case ACTION_MISS_TYPE_ACCURACY_MISS:
            case ACTION_MISS_TYPE_FORCED_FAILURE:
                if (action->miss_display_flags & ACTION_MISS_DISPLAY_DODGE_ANIMATION) {
                    battle_unit_face_towards_action_target(target, attacker);
                    animation = BATTLE_UNIT_ANIMATION_DODGE;
                }
                break;
            case ACTION_MISS_TYPE_BLADE_GRASP_OR_FINGER_GUARD:
            case ACTION_MISS_TYPE_CATCH:
                battle_unit_face_towards_action_target(target, attacker);
                animation = BATTLE_UNIT_ANIMATION_RAISE_HANDS;
                break;
            case ACTION_MISS_TYPE_CANCELED:
            case ACTION_MISS_TYPE_REFLECTED:
            case ACTION_MISS_TYPE_GOLEM_GUARD:
                break;
            default:
                if (action->hit != 0) {
                    if ((action->hp_damage != 0) || (action->mp_damage != 0)) {
                        animation = BATTLE_UNIT_ANIMATION_HURT;
                    } else if ((action->hp_healing != 0) || (action->mp_healing != 0)) {
                        animation = BATTLE_UNIT_ANIMATION_RAISE_HANDS;
                    }
                }
                break;
            }

            if (animation != 0) {
                if (target->spritesheet_id < BATTLE_SPRITESHEET_ID_CRYSTAL) {
                    battle_unit_store_animation_facing_movement_data(animation, target->facing, target);
                } else if (animation == BATTLE_UNIT_ANIMATION_HURT) {
                    battle_unit_store_animation_facing_movement_data(
                        BATTLE_UNIT_ANIMATION_HURT, target->facing, target);
                }
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_KNOCKBACK) {
                move_transfer_target_coordinates(attacker, target);
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_POACH) {
                target->status_flags_5_6 |= BATTLE_UNIT_STATUS_POACHED;
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_MORBOL) {
                target->status_flags_5_6 |= BATTLE_UNIT_STATUS_MORBOL;
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_BREAK_EQUIPMENT) {
                ai_init_unit_abilities(target->stats->battle_id);
            }
            if (action->special_effect & ACTION_SPECIAL_EFFECT_STEAL_ITEM) {
                ai_init_unit_abilities(target->stats->battle_id);
            }
            return;
        }
        animation = BATTLE_UNIT_ANIMATION_HURT;
    }
    battle_unit_store_animation_facing(animation, target->facing, target);
}

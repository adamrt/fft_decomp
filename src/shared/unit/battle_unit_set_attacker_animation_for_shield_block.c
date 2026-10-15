#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* When the target blocked with a hand or a golem took the blow, replace the
 * attacker's unarmed, swing or thrust animation with its recoil at the same
 * height. Only the TYPE1 and TYPE2 sheets have the recoils. */
void battle_unit_set_attacker_animation_for_shield_block(battle_unit_t* attacker, battle_unit_t* target) {
    unit_stats_t* stats;
    s32 kind;
    s32 anim;

    stats = target->stats;
    if (stats == 0) {
        return;
    }
    if (g_battle_gfx_spritesheet_data[attacker->spritesheet_id].shp_id >= BATTLE_SHP_CYOKO) {
        return;
    }
    kind = stats->action.miss_type;
    anim = 0;
    if (kind >= ACTION_MISS_TYPE_RIGHT_HAND_EVADE
        && (kind < ACTION_MISS_TYPE_CLASS_EVADE_OR_ARROW_GUARD || kind == ACTION_MISS_TYPE_GOLEM_GUARD)) {
        switch (attacker->animation.sequence_id >> 1) {
        case BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_HIGH:
            anim = BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_BLOCKED_HIGH;
            break;
        case BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_LEVEL:
            anim = BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_BLOCKED_LEVEL;
            break;
        case BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_LOW:
            anim = BATTLE_UNIT_ANIMATION_UNARMED_ATTACK_BLOCKED_LOW;
            break;
        case BATTLE_UNIT_ANIMATION_SWING_HIGH:
            anim = BATTLE_UNIT_ANIMATION_SWING_BLOCKED_HIGH;
            break;
        case BATTLE_UNIT_ANIMATION_SWING_LEVEL:
            anim = BATTLE_UNIT_ANIMATION_SWING_BLOCKED_LEVEL;
            break;
        case BATTLE_UNIT_ANIMATION_SWING_LOW:
            anim = BATTLE_UNIT_ANIMATION_SWING_BLOCKED_LOW;
            break;
        case BATTLE_UNIT_ANIMATION_THRUST_HIGH:
            anim = BATTLE_UNIT_ANIMATION_THRUST_BLOCKED_HIGH;
            break;
        case BATTLE_UNIT_ANIMATION_THRUST_LEVEL:
            anim = BATTLE_UNIT_ANIMATION_THRUST_BLOCKED_LEVEL;
            break;
        case BATTLE_UNIT_ANIMATION_THRUST_LOW:
            anim = BATTLE_UNIT_ANIMATION_THRUST_BLOCKED_LOW;
            break;
        }
    }
    if (anim != 0) {
        /* The target reads the u16 facing field signed (lh). */
        battle_unit_store_animation_facing_movement_data(anim, attacker->facing, attacker);
    }
}

#include "fft/battle.h"
#include "psx/types.h"

/* Classifies the attack direction and drops evades the target cannot use.
 *
 * The dominant axis of the attacker-to-target offset is compared with the
 * target's facing (position bits 8-11); on a diagonal the target counts as
 * hit from the front when it faces either axis toward the attacker, and a
 * shared tile counts as the back. Side and back attacks clear class evade,
 * back attacks also both shield evades. */
void battle_formula_calculate_facing_evade(void) {
    s32 dx;
    s32 dy;
    u8 distance_x;
    u8 distance_y;
    s32 direction;
    s32 facing;
    u8* result;

    dx = g_battle_action_target->x - g_battle_action_attacker->x;
    distance_x = dx < 0 ? -dx : dx;
    dy = g_battle_action_target->position.bits.y - g_battle_action_attacker->position.bits.y;
    distance_y = dy < 0 ? -dy : dy;
    direction = BATTLE_ABILITY_FACING_SIDE;
    if (distance_y < distance_x) {
        if (dx > 0) {
            facing = g_battle_action_target->position.raw & 0xf00;
            if (facing == 0x100) {
                direction = BATTLE_ABILITY_FACING_FRONT;
            } else if (facing == 0x300) {
                direction = BATTLE_ABILITY_FACING_BACK;
            }
        }
        if (dx < 0) {
            facing = g_battle_action_target->position.raw & 0xf00;
            if (facing == 0x300) {
                direction = BATTLE_ABILITY_FACING_FRONT;
            } else if (facing == 0x100) {
                direction = BATTLE_ABILITY_FACING_BACK;
            }
        }
    } else if (distance_x < distance_y) {
        if (dy > 0) {
            facing = g_battle_action_target->position.raw & 0xf00;
            if (facing == 0) {
                direction = BATTLE_ABILITY_FACING_FRONT;
            } else if (facing == 0x200) {
                direction = BATTLE_ABILITY_FACING_BACK;
            }
        }
        if (dy < 0) {
            facing = g_battle_action_target->position.raw & 0xf00;
            if (facing == 0x200) {
                direction = BATTLE_ABILITY_FACING_FRONT;
            } else if (facing == 0) {
                direction = BATTLE_ABILITY_FACING_BACK;
            }
        }
    } else if (dx > 0 && (g_battle_action_target->position.raw & 0xf00) == 0x100) {
        direction = BATTLE_ABILITY_FACING_FRONT;
    } else if (dx < 0 && (g_battle_action_target->position.raw & 0xf00) == 0x300) {
        direction = BATTLE_ABILITY_FACING_FRONT;
    } else if (dy > 0 && (g_battle_action_target->position.raw & 0xf00) == 0) {
        direction = BATTLE_ABILITY_FACING_FRONT;
    } else if (dy < 0 && (g_battle_action_target->position.raw & 0xf00) == 0x200) {
        direction = BATTLE_ABILITY_FACING_FRONT;
    } else if (dx == 0 && dy == 0) {
        direction = BATTLE_ABILITY_FACING_BACK;
    }
    result = &g_current_ability.facing_modifier;
    *result = direction;
    if (*result != BATTLE_ABILITY_FACING_FRONT) {
        g_current_ability.class_evade = 0;
    }
    if (*result == BATTLE_ABILITY_FACING_BACK) {
        g_current_ability.right_shield_evade = 0;
        g_current_ability.left_shield_evade = 0;
    }
}

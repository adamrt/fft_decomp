#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/data.h"
#include "psx/types.h"

/* Provisional: weapon attack animations, three per weapon type (low, level,
 * high). Bows index their row by battle_effect_get_bow_shot_angle. */

/* Choose the attacker's weapon swing animation from the target's height.
 *
 * The target treats spritesheet IDs at or above the Crystal sheet as always
 * using the level swing. Other units compare the vertical centres of both
 * sprites: more than 11 pixels apart selects the low or high swing. Golem
 * Guard and Blade Grasp targets are always swung at low. */
void battle_unit_select_weapon_attack_animation(battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target) {
    s32 miss_type;
    s16 attacker_centre;
    s16 target_centre;
    s32 difference;

    if (attacker == 0) {
        return;
    }
    if (target != 0) {
        if (attacker->spritesheet_id >= BATTLE_SPRITESHEET_ID_CRYSTAL) {
            battle_unit_store_animation_facing_movement_data(
                g_battle_weapon_attack_animations[0][1], (s16)attacker->facing, attacker);
            return;
        }
        if (attacker->equipped_weapon_type == ITEM_TYPE_BOW) {
            battle_unit_store_animation_facing_movement_data(
                g_battle_weapon_attack_animations[ITEM_TYPE_BOW][battle_effect_get_bow_shot_angle()],
                (s16)attacker->facing, attacker);
            return;
        }
        miss_type = target->battle_data->action.miss_type;
        if (miss_type != BATTLE_ACTION_MISS_TYPE_HIT) {
            if (miss_type >= BATTLE_ACTION_MISS_TYPE_REFLECTED) {
                if (miss_type < BATTLE_ACTION_MISS_TYPE_BUGGED_REFLECTION) {
                    if (miss_type >= BATTLE_ACTION_MISS_TYPE_GOLEM_GUARD) {
                        battle_unit_store_animation_facing_movement_data(
                            g_battle_weapon_attack_animations[attacker->equipped_weapon_type][0], (s16)attacker->facing,
                            attacker);
                        return;
                    }
                }
            }
        }
        /* The target loads the attacker's screen_z zero-extended (lhu). */
        attacker_centre
            = (s32)battle_gfx_get_unit_spritesheet_height_by_misc_id(attacker->unit_id) / 2 - (u16)attacker->screen.vy;
        target_centre = (s32)battle_gfx_get_unit_spritesheet_height_by_misc_id(target->unit_id) / 2 - target->screen.vy;
        difference = attacker_centre - target_centre;
        if (difference < -11) {
            battle_unit_store_animation_facing_movement_data(
                g_battle_weapon_attack_animations[attacker->equipped_weapon_type][0], (s16)attacker->facing, attacker);
            return;
        }
        if (difference >= 12) {
            battle_unit_store_animation_facing_movement_data(
                g_battle_weapon_attack_animations[attacker->equipped_weapon_type][2], (s16)attacker->facing, attacker);
            return;
        }
    }
    battle_unit_store_animation_facing_movement_data(
        g_battle_weapon_attack_animations[attacker->equipped_weapon_type][1], (s16)attacker->facing, attacker);
}

#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_select_attack_animation_for_ability(
    battle_unit_misc_data_t* attacker, battle_unit_misc_data_t* target) {
    s32 animation;
    s32 sequence_id;

    sequence_id = g_battle_gfx_spritesheet_data[attacker->spritesheet_id].seq_id;
    if (sequence_id < 8) {
        if (sequence_id >= 5) {
            /* facing is loaded signed (lh) here. */
            battle_unit_store_animation_facing(0x2c, *(s16*)&attacker->facing, attacker);
            return;
        }
    }

    animation = g_battle_ability_animation_data[attacker->used_ability_id].attack_animation;
    if (animation == BATTLE_ATTACK_ANIMATION_USE_WEAPON) {
        battle_unit_select_weapon_attack_animation(attacker, target);
        return;
    }
    if (animation == BATTLE_ATTACK_ANIMATION_USE_ITEM) {
        battle_unit_set_animation_for_item_abilities(attacker, target);
        return;
    }
    battle_unit_store_animation_facing(animation, *(s16*)&attacker->facing, attacker);
}

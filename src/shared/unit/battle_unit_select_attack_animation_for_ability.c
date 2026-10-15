#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Start the attacker's animation for the ability it uses: the cast for the
 * RUKA, ARUTE and KANZEN sequences, else the ability table's attack
 * animation, which may ask for the weapon's attack or the item animation. */
void battle_unit_select_attack_animation_for_ability(battle_unit_t* attacker, battle_unit_t* target) {
    s32 animation;
    s32 seq_type;

    seq_type = g_battle_gfx_spritesheet_data[attacker->spritesheet_id].seq_id;
    if (seq_type < 8) {
        if (seq_type >= BATTLE_SEQ_RUKA) {
            /* facing is loaded signed (lh) here. */
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_CAST, attacker->facing, attacker);
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
    battle_unit_store_animation_facing(animation, attacker->facing, attacker);
}

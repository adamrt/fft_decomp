#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"
#include "fft/sound.h"

/* Starts a unit's reaction animation to the casting unit's action
 * (battle_unit_set_target_animation_from_attack_type). Always returns 1. */
s32 battle_unit_set_target_animation_by_runtime_id(s32 runtime_id) {
    battle_unit_t* target = battle_unit_get_by_runtime_id(runtime_id);
    battle_unit_t* caster = battle_unit_get_casting();

    battle_unit_set_target_animation_from_attack_type(caster, target);
    return 1;
}

/* Starts the reaction animation of the unit in slot battle_id to the casting
 * unit's action. Always returns 1. No retail code calls it. */
s32 battle_unit_set_target_anim_by_battle_id(u16 battle_id) {
    battle_unit_t* target = battle_unit_get_by_battle_id(battle_id);
    battle_unit_t* caster = battle_unit_get_casting();

    battle_unit_set_target_animation_from_attack_type(caster, target);
    return 1;
}

/* battle_gfx_prepare_post_action_display for a runtime id. An absent unit raises
 * pointer exception 0xc and returns 0. */
s32 battle_gfx_prepare_post_action_display_by_runtime_id(s32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit != 0) {
        battle_gfx_prepare_post_action_display(unit);
        return 1;
    }
    main_system_handle_pointer_exception(0xc);
    return 0;
}

/* Lands a projectile strike on its target: the weapon hit sound, the target's
 * reaction animation, its post-action display and the evade and weapon-element
 * effect data. Does nothing when either unit is missing. */
void battle_unit_set_bow_hardcoding(battle_unit_t* attacker, battle_unit_t* target) {
    if ((attacker != 0) && (target != 0)) {
        sound_play_weapon_sfx(attacker, 1);
        battle_unit_set_target_animation_from_attack_type(attacker, target);
        battle_gfx_prepare_post_action_display_by_runtime_id(target->runtime_id);
        effect_set_evade_type_data_and_weapon_element(attacker, 1);
    }
}

/* battle_unit_set_bow_hardcoding for two battle ids. No retail code calls it. */
void battle_unit_call_bow_hardcoding_by_battle_id(u16 attacker_id, u16 target_id) {
    battle_unit_t* attacker = battle_unit_get_by_battle_id(attacker_id);
    battle_unit_t* target = battle_unit_get_by_battle_id(target_id);

    battle_unit_set_bow_hardcoding(attacker, target);
}

/* battle_unit_set_bow_hardcoding for two unit unit ids: called by the arrow-arc
 * and linear projectile effects for the unit the projectile hits. */
void battle_unit_call_bow_hardcoding_by_runtime_id(u16 attacker_id, u16 target_id) {
    battle_unit_t* attacker = battle_unit_get_by_runtime_id(attacker_id);
    battle_unit_t* target = battle_unit_get_by_runtime_id(target_id);

    battle_unit_set_bow_hardcoding(attacker, target);
}

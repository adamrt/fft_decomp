#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"
#include "fft/sound.h"

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

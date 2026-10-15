#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/data.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/*
 * Select the effect file for an ability and start its charge secondary effect.
 *
 * `packed` carries the ability's charge animation set in its low 16 bits and
 * its element mask in its high 16 bits. `ability` is an ability id or, from
 * ABILITY_ID_COUNT up, one of the effects past the abilities. A thrown ball
 * (ABILITY_ID_THROW_BALL) takes the Fire, Ice or Bolt effect by the ball's
 * element. A negative table entry means the ability has no effect file: only
 * the two fixed secondary animations for the equipment-break and Charge
 * ability ranges are started, and nothing is retained.
 */
void effect_set_ability_animation(u32 packed, s32 ability, effect_secondary_init_t* source) {
    g_effect_load_state = EFFECT_LOAD_START;
    if (ability < ABILITY_ID_COUNT) {
        if (ability == ABILITY_ID_THROW_BALL) {
            s32 ball = effect_get_element_index(packed >> 16);
            if (ball == EFFECT_ELEMENT_INDEX_FIRE)
                g_battle_ability_effect_id = EFFECT_FILE_FIRE;
            else if (ball == EFFECT_ELEMENT_INDEX_WATER)
                g_battle_ability_effect_id = EFFECT_FILE_ICE;
            else if (ball == EFFECT_ELEMENT_INDEX_LIGHTNING)
                g_battle_ability_effect_id = EFFECT_FILE_BOLT;
        } else {
            g_battle_ability_effect_id = g_effect_ability_files[ability];
        }
    } else {
        /* Event IDs continue after the action/reaction ability rows. */
        g_battle_ability_effect_id = g_effect_ability_files[ability - ABILITY_ID_COUNT + ABILITY_ID_SUPPORT_FIRST];
    }

    if (g_battle_ability_effect_id >= 0) {
        g_battle_ability_effect_id &= EFFECT_ABILITY_FILE_MASK;
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_REQUESTED;
        if (g_effect_secondary_by_charge_animation[packed & 0xffff] != 0) {
            g_effect_charge_secondary_id
                = effect_init_secondary(g_effect_secondary_by_charge_animation[packed & 0xffff], packed >> 16, source);
        } else {
            g_effect_charge_secondary_id = 0;
        }
        g_effect_sound_call_counts[3] = 0;
        g_effect_sound_call_counts[2] = 0;
        g_effect_sound_call_counts[1] = 0;
        g_effect_sound_call_counts[0] = 0;
        g_effect_context = *source;
        g_battle_unknown_801bc0d4 = 0;
        return;
    }

    if (ability >= ABILITY_ID_BATTLE_SKILL_HEAD_BREAK && ability <= ABILITY_ID_BATTLE_SKILL_MIND_BREAK)
        effect_init_secondary(EFFECT_SECONDARY_ANIMATION_EQUIP_BREAK, 0, source);
    if ((ability >= ABILITY_ID_CHARGE_FIRST && ability < ABILITY_ID_MATH_FIRST)
        || ability == ABILITY_ID_BASIC_SKILL_ACCUMULATE)
        effect_init_secondary(EFFECT_SECONDARY_ANIMATION_RED_ORBS, 0, source);
}

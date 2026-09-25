#include "fft/battle.h"

/*
 * Dispatch one keyframe's action flags to a single effect target: bits 4-6
 * and 8-15 trigger unit reactions (bit 3 selects the caster entry), bits 0-2
 * register a callback slot.
 */
void battle_effect_run_keyframe_actions(u16 flags, u8 target, battle_keyframe_effect_state_t* state) {
    s32 misc_id;
    u32 animation;
    u16 slot;

    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
        misc_id = -1;
        if (g_battle_effect_targets[16].target_type == 0) {
            misc_id = g_battle_effect_targets[16].id.misc_id;
        }
    } else {
        misc_id = -1;
        if (g_battle_effect_targets[target].target_type == 0) {
            misc_id = g_battle_effect_targets[target].id.misc_id;
        }
    }
    /* These three calls pass misc_id without the u16 zero-extension the prototypes add. */
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY) && misc_id != -1) {
        ((s32 (*)(s32))battle_gfx_prepare_post_action_display_by_misc_id)(misc_id);
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY) && misc_id != -1) {
        ((void (*)(s32))battle_unit_update_display_by_misc_id)(misc_id);
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION) && misc_id != -1) {
        ((s32 (*)(s32))battle_unit_set_target_anim_by_misc_id)(misc_id);
    }
    animation = (u32)(flags & EFFECT_KEYFRAME_ACTION_ANIMATION_MASK) >> EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT;
    if (animation != 0 && misc_id != -1) {
        battle_unit_set_specific_animation_by_misc_id(misc_id, animation);
    }
    if (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) {
        slot = (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) - 1;
        state->callback_state[slot] = 1;
        state->callback_ptrs[slot] = g_battle_effect_callback_slots[slot];
    }
}

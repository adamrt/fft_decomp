#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/*
 * Dispatch one keyframe's action flags to a single effect target: bits 4-6
 * and 8-15 trigger unit reactions (bit 3 selects the caster entry), bits 0-2
 * register a callback slot.
 */
void effect_run_keyframe_actions(u16 flags, u8 target, effect_record_t* state) {
    s32 runtime_id;
    u32 animation;
    u16 slot;

    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
        runtime_id = -1;
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            runtime_id = g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id;
        }
    } else {
        runtime_id = -1;
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            runtime_id = g_effect_context.targets[target].data.unit.runtime_id;
        }
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY) && runtime_id != -1) {
        battle_gfx_prepare_post_action_display_by_runtime_id(runtime_id);
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY) && runtime_id != -1) {
        battle_unit_update_display_by_runtime_id(runtime_id);
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION) && runtime_id != -1) {
        battle_unit_set_target_animation_by_runtime_id(runtime_id);
    }
    animation = (flags & EFFECT_KEYFRAME_ACTION_ANIMATION_MASK) >> EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT;
    if (animation != 0 && runtime_id != -1) {
        battle_unit_set_specific_animation_by_runtime_id(runtime_id, animation);
    }
    if (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) {
        slot = (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) - 1;
        state->callback_phase[slot] = EFFECT_CALLBACK_PHASE_INIT;
        state->callbacks[slot] = g_effect_callback_slots[slot];
    }
}

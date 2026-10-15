#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/*
 * Dispatch one keyframe's action flags to every live effect target: bits 4-6
 * and 8-15 trigger unit reactions, and bit 3 restricts each of them to the
 * caster entry instead of the whole target list; bits 0-2 register a callback
 * slot.
 */
void effect_run_keyframe_actions_all_targets(u16 flags, effect_record_t* state) {
    s32 runtime_id;
    u16 animation;
    u16 slot;
    s32 i;

    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
        runtime_id = -1;
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            runtime_id = g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id;
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                battle_gfx_prepare_post_action_display_by_runtime_id(runtime_id);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    battle_gfx_prepare_post_action_display_by_runtime_id(
                        g_effect_context.targets[i].data.unit.runtime_id);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                battle_unit_update_display_by_runtime_id(runtime_id);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    battle_unit_update_display_by_runtime_id(g_effect_context.targets[i].data.unit.runtime_id);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                battle_unit_set_target_animation_by_runtime_id(runtime_id);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    battle_unit_set_target_animation_by_runtime_id(g_effect_context.targets[i].data.unit.runtime_id);
                }
            }
        }
    }
    animation = (flags & EFFECT_KEYFRAME_ACTION_ANIMATION_MASK) >> EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT;
    if (animation != 0) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (runtime_id != -1) {
                battle_unit_set_specific_animation_by_runtime_id(runtime_id, animation);
            }
        } else {
            for (i = 0; i < g_effect_context.target_count; i++) {
                if (g_effect_context.targets[i].target_type == EFFECT_TARGET_TYPE_UNIT) {
                    battle_unit_set_specific_animation_by_runtime_id(
                        g_effect_context.targets[i].data.unit.runtime_id, animation);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) {
        slot = (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) - 1;
        state->callback_phase[slot] = EFFECT_CALLBACK_PHASE_INIT;
        state->callbacks[slot] = g_effect_callback_slots[slot];
    }
}

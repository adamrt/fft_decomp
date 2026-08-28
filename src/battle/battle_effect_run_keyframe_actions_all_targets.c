#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/effect.h"

/*
 * Dispatch one keyframe's action flags to every live effect target: bits 4-6
 * and 8-15 trigger unit reactions, and bit 3 restricts each of them to the
 * caster entry instead of the whole target list; bits 0-2 register a callback
 * slot.
 */
void battle_effect_run_keyframe_actions_all_targets(u16 flags, battle_keyframe_effect_state_t* state) {
    s32 misc_id;
    u16 animation;
    u16 slot;
    s32 i;

    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
        misc_id = -1;
        if (g_battle_effect_targets[16].target_type == 0) {
            misc_id = g_battle_effect_targets[16].id.misc_id;
        }
    }
    /* The misc_id calls below skip the u16 zero-extension their prototypes add. */
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (misc_id != -1) {
                ((s32 (*)(s32))battle_gfx_prepare_post_action_display_by_misc_id)(misc_id);
            }
        } else {
            for (i = 0; i < g_battle_effect_coord_data.hit_counter; i++) {
                if (g_battle_effect_targets[i].target_type == 0) {
                    battle_gfx_prepare_post_action_display_by_misc_id(g_battle_effect_targets[i].id.misc_id);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (misc_id != -1) {
                ((void (*)(s32))battle_unit_update_display_by_misc_id)(misc_id);
            }
        } else {
            for (i = 0; i < g_battle_effect_coord_data.hit_counter; i++) {
                if (g_battle_effect_targets[i].target_type == 0) {
                    battle_unit_update_display_by_misc_id(g_battle_effect_targets[i].id.misc_id);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (misc_id != -1) {
                ((s32 (*)(s32))battle_unit_set_target_anim_by_misc_id)(misc_id);
            }
        } else {
            for (i = 0; i < g_battle_effect_coord_data.hit_counter; i++) {
                if (g_battle_effect_targets[i].target_type == 0) {
                    ((s32 (*)(s32))battle_unit_set_target_anim_by_misc_id)(g_battle_effect_targets[i].id.misc_id);
                }
            }
        }
    }
    animation = (flags & EFFECT_KEYFRAME_ACTION_ANIMATION_MASK) >> EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT;
    if (animation != 0) {
        if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
            if (misc_id != -1) {
                battle_unit_set_specific_animation_by_misc_id(misc_id, animation);
            }
        } else {
            for (i = 0; i < g_battle_effect_coord_data.hit_counter; i++) {
                if (g_battle_effect_targets[i].target_type == 0) {
                    battle_unit_set_specific_animation_by_misc_id(g_battle_effect_targets[i].id.misc_id, animation);
                }
            }
        }
    }
    if (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) {
        slot = (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) - 1;
        state->callback_state[slot] = 1;
        state->callback_ptrs[slot] = g_battle_effect_callback_slots[slot];
    }
}

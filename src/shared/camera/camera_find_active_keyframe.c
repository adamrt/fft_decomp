#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

/* Find the first camera keyframe of the tracks in mask that is still ahead
 * of frame, reporting (table << 16 | index) and (target << 16 | end frame).
 *
 * The phase 1 table covers frames before phase1_duration; the child table
 * repeats every spawn_delay frames per effect target (current target, then
 * the next one), and once more for the last target within child_duration; the
 * phase 2 table is relative to the end of that window.
 *
 * The conditional-expression header reads keep the entry block from caching
 * the unit-data pointer. */
s32 camera_find_active_keyframe(s16 frame, s16 mask, s32* out_index, s32* out_frame) {
    s32 phase1;
    s32 spawn_delay;
    s32 child_duration;
    s32 i;
    s32 target_index;
    s32 local_frame;
    s32 elapsed;
    s32 start;

    phase1 = g_effect_timeline->phase1_duration;
    spawn_delay = g_effect_context.target_count != 0 ? g_effect_timeline->spawn_delay : 0;
    child_duration = g_effect_context.target_count != 0 ? g_effect_timeline->child.duration : 0;

    if (frame < phase1) {
        for (i = 0; i <= g_effect_timeline->phase1_camera.last_keyframe_index; i++) {
            if (frame < g_effect_timeline->phase1_camera.end_frame[i]
                && (mask & g_effect_timeline->phase1_camera.command[i]) != 0) {
                *out_index = i;
                *out_frame = g_effect_timeline->phase1_camera.end_frame[i];
                return 1;
            }
        }
    }

    if (spawn_delay != 0 && frame < phase1 + (g_effect_context.target_count - 1) * spawn_delay) {
        target_index = 0;
        if (frame >= phase1) {
            elapsed = frame - phase1;
            target_index = elapsed / spawn_delay;
            local_frame = elapsed - target_index * spawn_delay;
        } else {
            local_frame = -1;
        }
        for (i = 0; i <= g_effect_timeline->last_keyframe_index; i++) {
            if (g_effect_timeline->end_frame[i] >= spawn_delay) {
                break;
            }
            if (local_frame < g_effect_timeline->end_frame[i] && (mask & g_effect_timeline->command[i]) != 0) {
                *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                *out_frame
                    = (target_index << 16) + (g_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                return 1;
            }
        }
        target_index++;
        local_frame = -1;
        if (target_index < g_effect_context.target_count) {
            for (i = 0; i <= g_effect_timeline->last_keyframe_index; i++) {
                if (g_effect_timeline->end_frame[i] >= spawn_delay) {
                    break;
                }
                if (local_frame < g_effect_timeline->end_frame[i] && (mask & g_effect_timeline->command[i]) != 0) {
                    *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                    *out_frame = (target_index << 16)
                        + (g_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                    return 1;
                }
            }
        }
    }

    if (g_effect_context.target_count != 0) {
        target_index = g_effect_context.target_count - 1;
        start = phase1 + target_index * spawn_delay;
        if (frame < start + child_duration) {
            local_frame = -1;
            if (frame >= start) {
                local_frame = frame - phase1 - target_index * spawn_delay;
            }
            for (i = 0; i <= g_effect_timeline->last_keyframe_index; i++) {
                if (local_frame < g_effect_timeline->end_frame[i] && (mask & g_effect_timeline->command[i]) != 0) {
                    *out_index = i + (EFFECT_CAMERA_TABLE_CHILD << 16);
                    *out_frame = (target_index << 16)
                        + (g_effect_timeline->end_frame[i] + target_index * spawn_delay + phase1);
                    return 1;
                }
            }
        }
    }

    if (frame >= phase1 + (g_effect_context.target_count - 1) * spawn_delay + child_duration) {
        local_frame = frame - phase1 - (g_effect_context.target_count - 1) * spawn_delay - child_duration;
    } else {
        local_frame = -1;
    }
    for (i = 0; i <= g_effect_timeline->phase2_camera.last_keyframe_index; i++) {
        if (local_frame < g_effect_timeline->phase2_camera.end_frame[i]
            && (mask & g_effect_timeline->phase2_camera.command[i]) != 0) {
            *out_index = i + (EFFECT_CAMERA_TABLE_PHASE2 << 16);
            *out_frame = g_effect_timeline->phase2_camera.end_frame[i] + phase1
                + (g_effect_context.target_count - 1) * spawn_delay + child_duration;
            return 1;
        }
    }
    return 0;
}

#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "psx/types.h"

/* Find the first camera keyframe of the channels in mask that is still ahead
 * of frame, reporting (table << 16 | index) and (target << 16 | end frame).
 *
 * The main table covers frames before phase1_duration; the for-each-target
 * table repeats every spawn_delay frames per hit target (current target, then
 * the next one), and once more for the last target within phase2_delay; the
 * cleanup table is relative to the end of that window.
 * Also known as find_active_camera_keyframe.
 *
 * The target places each found block after the scans and before the cleanup
 * scan, so they are written as goto targets in that order; the three
 * for-each-target copies are separate in the target too (jump2 merges only
 * their tails). The conditional-expression header reads keep the entry block
 * from caching the misc-data pointer. */
s32 battle_camera_find_active_keyframe(s16 frame, s32 mask, s32* out_index, s32* out_frame) {
    effect_misc_data_t* data;
    s32 phase1;
    s32 spawn_delay;
    s32 phase2_delay;
    s32 i;
    s32 target_index;
    s32 local_frame;
    s32 elapsed;
    s32 start;

    phase1 = g_battle_effect_misc_data->phase1_duration;
    spawn_delay = g_battle_effect_coord_data.hit_counter != 0 ? g_battle_effect_misc_data->spawn_delay : 0;
    phase2_delay = g_battle_effect_coord_data.hit_counter != 0 ? g_battle_effect_misc_data->phase2_delay : 0;

    if (frame < phase1) {
        data = g_battle_effect_misc_data;
        for (i = 0; i <= data->main.last_keyframe_index; i++) {
            if (frame < data->main.frame_thresholds[i] && (mask & data->main.selector[i]) != 0) {
                goto found_main;
            }
            data = g_battle_effect_misc_data;
        }
    }

    if (spawn_delay != 0 && frame < phase1 + (g_battle_effect_coord_data.hit_counter - 1) * spawn_delay) {
        target_index = 0;
        if (frame >= phase1) {
            elapsed = frame - phase1;
            target_index = elapsed / spawn_delay;
            local_frame = elapsed - target_index * spawn_delay;
        } else {
            local_frame = -1;
        }
        data = g_battle_effect_misc_data;
        for (i = 0; i <= data->last_keyframe_index; i++) {
            if (data->frame_thresholds[i] >= spawn_delay) {
                break;
            }
            if (local_frame < data->frame_thresholds[i] && (mask & data->selector[i]) != 0) {
                goto found_target;
            }
            data = g_battle_effect_misc_data;
        }
        target_index++;
        local_frame = -1;
        if (target_index < g_battle_effect_coord_data.hit_counter) {
            data = g_battle_effect_misc_data;
            for (i = 0; i <= data->last_keyframe_index; i++) {
                if (data->frame_thresholds[i] >= spawn_delay) {
                    break;
                }
                if (local_frame < data->frame_thresholds[i] && (mask & data->selector[i]) != 0) {
                    goto found_next_target;
                }
                data = g_battle_effect_misc_data;
            }
        }
    }

    if (g_battle_effect_coord_data.hit_counter != 0) {
        target_index = g_battle_effect_coord_data.hit_counter - 1;
        start = phase1 + target_index * spawn_delay;
        if (frame < start + phase2_delay) {
            local_frame = -1;
            if (frame >= start) {
                local_frame = frame - phase1 - target_index * spawn_delay;
            }
            data = g_battle_effect_misc_data;
            for (i = 0; i <= data->last_keyframe_index; i++) {
                if (local_frame < data->frame_thresholds[i] && (mask & data->selector[i]) != 0) {
                    goto found_last_target;
                }
                data = g_battle_effect_misc_data;
            }
        }
    }

    local_frame = -1;
    if (frame >= phase1 + (g_battle_effect_coord_data.hit_counter - 1) * spawn_delay + phase2_delay) {
        local_frame = frame - phase1 - (g_battle_effect_coord_data.hit_counter - 1) * spawn_delay - phase2_delay;
    }
    goto scan_cleanup;

found_main:
    *out_index = i;
    *out_frame = g_battle_effect_misc_data->main.frame_thresholds[i];
    return 1;

found_target:
    *out_index = i + 0x10000;
    *out_frame
        = (target_index << 16) + (g_battle_effect_misc_data->frame_thresholds[i] + target_index * spawn_delay + phase1);
    return 1;

found_next_target:
    *out_index = i + 0x10000;
    *out_frame
        = (target_index << 16) + (g_battle_effect_misc_data->frame_thresholds[i] + target_index * spawn_delay + phase1);
    return 1;

found_last_target:
    *out_index = i + 0x10000;
    *out_frame
        = (target_index << 16) + (g_battle_effect_misc_data->frame_thresholds[i] + target_index * spawn_delay + phase1);
    return 1;

found_cleanup:
    *out_index = i + 0x20000;
    *out_frame = g_battle_effect_misc_data->cleanup.frame_thresholds[i] + phase1
        + (g_battle_effect_coord_data.hit_counter - 1) * spawn_delay + phase2_delay;
    return 1;

scan_cleanup:
    data = g_battle_effect_misc_data;
    for (i = 0; i <= data->cleanup.last_keyframe_index; i++) {
        if (local_frame < data->cleanup.frame_thresholds[i] && (mask & data->cleanup.selector[i]) != 0) {
            goto found_cleanup;
        }
        data = g_battle_effect_misc_data;
    }
    return 0;
}

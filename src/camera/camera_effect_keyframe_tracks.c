/* Effect camera keyframe search and the per-frame rotation/position/zoom track starts for both timelines. */

#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

/*
 * Finds the first keyframe of the child camera table that ends after `frame`
 * and moves a track in `mask` (EFFECT_CAMERA_TRACK_*), reporting its
 * index and end frame. Nothing is found once the child timeline has run its
 * child_duration.
 *
 * The table pointer is re-read at the bottom of the loop, which is what stops
 * loop.c from turning the indexed reads into a single advancing pointer.
 */
s32 effect_find_next_keyframe(s16 frame, s16 mask, s32* out_index, s32* out_frame) {
    effect_timeline_t* data;
    s32 i;

    data = g_effect_timeline;
    if (frame < data->child.duration) {
        i = 0;
        if (data->last_keyframe_index >= 0) {
            do {
                if (frame < data->end_frame[i] && (mask & data->command[i]) != 0) {
                    *out_index = i;
                    *out_frame = g_effect_timeline->end_frame[i];
                    return 1;
                }
                i++;
                data = g_effect_timeline;
            } while (i <= data->last_keyframe_index);
        }
    }
    return 0;
}

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

/* Starts the child timeline's camera moves for this frame: each of the
 * rotation, position and zoom tracks that is idle takes its next keyframe
 * from the child camera table. */
void camera_start_effect_keyframe_moves(s16 frame) {
    SVECTOR rotation;
    VECTOR vector;
    s32 index;
    s32 keyframe;

    if (g_effect_camera_rotation_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_ROTATION, &index, &keyframe) != 0) {
        effect_load_svector(&g_effect_timeline->rotation[index], &rotation);
        camera_execute_angle_command(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &rotation);
    }
    if (g_effect_camera_position_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_POSITION, &index, &keyframe) != 0) {
        effect_load_vector(&g_effect_timeline->position[index], &vector);
        camera_execute_position_command(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
    if (g_effect_camera_zoom_mode == EFFECT_CAMERA_MODE_IDLE
        && effect_find_next_keyframe(frame, EFFECT_CAMERA_TRACK_ZOOM, &index, &keyframe) != 0) {
        effect_load_vector(&g_effect_timeline->zoom[index], &vector);
        camera_execute_zoom_command(
            g_effect_timeline->command[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
}

/* Starts the parent timeline's camera moves for this frame: each of the
 * rotation, position and zoom tracks that is idle takes its next keyframe
 * from the table camera_find_active_keyframe reports (phase 1, child
 * or phase 2). The three-table form of
 * camera_start_effect_keyframe_moves. */
void camera_advance_effect_tracks(s16 frame) {
    SVECTOR rotation;
    VECTOR vector;
    s32 index;
    s32 keyframe;
    s32 k;

    if (g_effect_camera_rotation_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_ROTATION, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_load_svector(&g_effect_timeline->phase1_camera.rotation[k], &rotation);
            camera_execute_angle_command(g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &rotation);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_load_svector(&g_effect_timeline->rotation[k], &rotation);
            camera_execute_angle_command(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &rotation);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_load_svector(&g_effect_timeline->phase2_camera.rotation[k], &rotation);
            camera_execute_angle_command(g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000,
                (keyframe & 0xffff) - frame, &rotation);
            break;
        }
    }
    if (g_effect_camera_position_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_POSITION, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_load_vector(&g_effect_timeline->phase1_camera.position[k], &vector);
            camera_execute_position_command(
                g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_load_vector(&g_effect_timeline->position[k], &vector);
            camera_execute_position_command(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_load_vector(&g_effect_timeline->phase2_camera.position[k], &vector);
            camera_execute_position_command(
                g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        }
    }
    if (g_effect_camera_zoom_mode == EFFECT_CAMERA_MODE_IDLE
        && camera_find_active_keyframe(frame, EFFECT_CAMERA_TRACK_ZOOM, &index, &keyframe) != 0) {
        k = index & 0xffff;
        switch (index / 0x10000) {
        case EFFECT_CAMERA_TABLE_PHASE1:
            effect_load_vector(&g_effect_timeline->phase1_camera.zoom[k], &vector);
            camera_execute_zoom_command(
                g_effect_timeline->phase1_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_CHILD:
            effect_load_vector(&g_effect_timeline->zoom[k], &vector);
            camera_execute_zoom_command(
                g_effect_timeline->command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        case EFFECT_CAMERA_TABLE_PHASE2:
            effect_load_vector(&g_effect_timeline->phase2_camera.zoom[k], &vector);
            camera_execute_zoom_command(
                g_effect_timeline->phase2_camera.command[k], keyframe / 0x10000, (keyframe & 0xffff) - frame, &vector);
            break;
        }
    }
}

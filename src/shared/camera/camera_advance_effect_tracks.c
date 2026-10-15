#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

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

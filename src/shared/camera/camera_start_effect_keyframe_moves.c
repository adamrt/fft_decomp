#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/types.h"

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

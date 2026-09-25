#include "fft/battle.h"
#include "psx/types.h"

/* The selector halfwords are u16 in effect_misc_data_t (the keyframe scanner
 * masks them); this dispatcher sign-extends them, so each read casts to s16
 * to keep the `lh`. */

/* Starts the camera position, target and zoom moves toward the next effect
 * keyframe of each channel that is not locked by g_battle_effect_camera_rotation_mode/D0/D4. */
void battle_camera_start_effect_keyframe_moves(s16 frame) {
    SVECTOR position;
    VECTOR vector;
    s32 index;
    s32 keyframe;

    if (g_battle_effect_camera_rotation_mode == 0
        && battle_effect_find_next_keyframe(frame, 1, &index, &keyframe) != 0) {
        battle_effect_copy_svector((u16*)g_battle_effect_misc_data->position[index], (u16*)&position);
        battle_camera_execute_angle_command((s16)g_battle_effect_misc_data->selector[index], keyframe / 0x10000,
            (keyframe & 0xFFFF) - frame, &position);
    }
    if (g_battle_effect_camera_position_mode == 0
        && battle_effect_find_next_keyframe(frame, 2, &index, &keyframe) != 0) {
        battle_effect_convert_svector_to_vector(g_battle_effect_misc_data->target[index], &vector);
        battle_camera_execute_position_command(
            (s16)g_battle_effect_misc_data->selector[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
    if (g_battle_effect_camera_zoom_mode == 0 && battle_effect_find_next_keyframe(frame, 4, &index, &keyframe) != 0) {
        battle_effect_convert_svector_to_vector(g_battle_effect_misc_data->zoom[index], &vector);
        battle_camera_execute_zoom_command(
            (s16)g_battle_effect_misc_data->selector[index], keyframe / 0x10000, (keyframe & 0xFFFF) - frame, &vector);
    }
}

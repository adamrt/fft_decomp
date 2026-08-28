#include "fft/battle_effect.h"
#include "psx/types.h"

/*
 * Finds the first keyframe strictly after `frame` whose selector mask
 * intersects `mask`, reporting its index and frame.
 *
 * The table pointer is re-read at the bottom of the loop, which is what stops
 * loop.c from turning the indexed reads into a single advancing pointer.
 */
s32 battle_effect_find_next_keyframe(s16 frame, u32 mask, s32* out_index, s32* out_frame) {
    effect_misc_data_t* data;
    s32 i;

    data = g_battle_effect_misc_data;
    if (frame < (s32)data->phase2_delay) {
        i = 0;
        if (data->last_keyframe_index >= 0) {
            do {
                if (frame < data->frame_thresholds[i] && (mask & data->selector[i]) != 0) {
                    *out_index = i;
                    *out_frame = g_battle_effect_misc_data->frame_thresholds[i];
                    return 1;
                }
                i++;
                data = g_battle_effect_misc_data;
            } while (i <= data->last_keyframe_index);
        }
    }
    return 0;
}

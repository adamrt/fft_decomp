#include "fft/open.h"

/*
 * Advance the script-controlled screen fade and tint the active fade quad.
 * Block-local control pointers retain the original materialized flag addresses.
 */
void open_script_update_screen_fade(void) {
    s32 intensity;

    {
        u32* control = &g_open_script_state.dispatch.flags;
        u32 flags;

        flags = *control;
        if (flags & OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY) {
            s32 elapsed_frames = g_open_script_state.opntex.fade_elapsed_frames + 1;
            s32 duration_frames = g_open_script_state.opntex.fade_duration_frames;

            intensity = (elapsed_frames << 7) / duration_frames;
            g_open_script_state.opntex.fade_elapsed_frames = elapsed_frames;
            if (intensity >= 0x81) {
                intensity = 0x80;
            }
            g_open_script_state.opntex.fade_intensity = intensity;
            if (elapsed_frames >= duration_frames) {
                g_open_script_state.opntex.fade_intensity = 0x80;
                *control = flags ^ OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY;
            }
        }
    }
    {
        u32* control = &g_open_script_state.dispatch.flags;
        u32 flags;

        flags = *control;
        if (flags & OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY) {
            s32 elapsed_frames = g_open_script_state.opntex.fade_elapsed_frames + 1;
            s32 duration_frames = g_open_script_state.opntex.fade_duration_frames;

            g_open_script_state.opntex.fade_intensity = 0x80;
            g_open_script_state.opntex.fade_elapsed_frames = elapsed_frames;
            intensity = 0x80 - (elapsed_frames << 7) / duration_frames;
            if (intensity < 0) {
                intensity = 0;
            }
            g_open_script_state.opntex.fade_intensity = intensity;
            if (elapsed_frames >= duration_frames) {
                g_open_script_state.opntex.fade_intensity = 0;
                *control = flags ^ OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY;
            }
        }
    }

    intensity = g_open_script_state.opntex.fade_intensity;
    setRGB0(&g_open_gfx_fade_primitives[g_active_graphics_buffer_index], intensity, intensity, intensity);
}

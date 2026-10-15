#include "fft/open.h"
#include "psx/types.h"

/* Advance the OPNTEX sequence: run down the fade delay of the frame on
 * screen, and when the frame countdown expires load the next frame and start
 * its fade (or stop the sequence after the last frame). */
void open_gfx_update_opntex_sequence(void) {
    s32 frame;
    u32* source;

    if (g_open_script_state.opntex.frame_duration == 0) {
        return;
    }

    if (g_open_script_state.opntex.delay != 0) {
        g_open_script_state.opntex.delay--;
        if (g_open_script_state.opntex.delay == 0) {
            g_open_script_state.dispatch.flags |= OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY;
            g_open_script_state.opntex.fade_intensity = 0x80;
            g_open_script_state.opntex.fade_elapsed_frames = 0;
            g_open_script_state.opntex.fade_duration_frames = g_open_script_state.opntex_control_value;
        }
    }

    if (--g_open_script_state.opntex.frame_countdown != 0) {
        return;
    }

    frame = g_open_script_state.opntex.first_frame;
    if (g_open_script_state.opntex.last_frame < frame) {
        g_open_script_state.opntex.frame_duration = 0;
        return;
    }

    source = g_open_gfx_opntex_data;
    source += frame * 23 * 512;
    open_gfx_load_opntex_into_frame_buffer(source);
    g_open_script_state.dispatch.flags |= OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY;
    g_open_script_state.opntex.fade_elapsed_frames = 0;
    g_open_script_state.opntex.fade_intensity = 0;
    g_open_script_state.opntex.fade_duration_frames = g_open_script_state.opntex_control_value;
    g_open_script_state.opntex.first_frame++;
    g_open_script_state.opntex.frame_countdown = g_open_script_state.opntex.frame_duration;
    g_open_script_state.opntex.delay
        = g_open_script_state.opntex.frame_duration - g_open_script_state.opntex_control_value;
}

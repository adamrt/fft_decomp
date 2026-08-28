#include "fft/main_runtime.h"
#include "fft/open.h"
#include "psx/gte.h"

/*
 * Advance the script-controlled screen fade and tint the active fade quad.
 *
 * Each fade direction reads the dispatch flags through its own block-local
 * pointer, which keeps the target's materialised flag address. The colour
 * bytes are written through the quad's r0/g0/b0 word viewed as a CVECTOR:
 * combine folds those address pseudos into the byte stores and leaves two of
 * them USE-only, which gives the target's unreferenced 16-byte leaf frame.
 */
void open_script_update_screen_fade(void) {
    s32 intensity;

    {
        volatile u32* control = &g_open_script_state.dispatch.flags;
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
        volatile u32* control = &g_open_script_state.dispatch.flags;
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
    ((CVECTOR*)&g_open_gfx_fade_primitives[g_active_graphics_buffer_index].r0)->r = intensity;
    ((CVECTOR*)&g_open_gfx_fade_primitives[g_active_graphics_buffer_index].r0)->g = intensity;
    ((CVECTOR*)&g_open_gfx_fade_primitives[g_active_graphics_buffer_index].r0)->b = intensity;
}

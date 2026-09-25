#include "fft/open.h"
#include "psx/types.h"

/* Volatile scalar view of g_open_script_state.dispatch.flags: the struct
 * member makes GCC hold the flags address in a register, but the target
 * addresses the flags by symbol for both the load and the store. */
extern volatile u32 g_open_script_dispatch_flags;

/* Advance the OPNTEX sequence and start a fade for each loaded frame.
 *
 * The target bases several state accesses, including the distant script flags
 * at -0x5a6 words, on the frame-duration pointer. Replacing the fade-intensity
 * store with a named field emits an extra `lui` and breaks the exact match;
 * these indexed accesses preserve the target address sequence. */
void open_gfx_update_opntex_sequence(void) {
    volatile s32* duration_pointer = &g_open_script_state.opntex.frame_duration;
    s32* countdown_pointer;
    s32 delay;
    s32 countdown;
    s32 frame;
    s32 control_value;
    const u8* source;

    /* Hides the pointer's constant value; without it every access below folds to an absolute lui/%lo address. */
    __asm__ volatile("" : "=r"(duration_pointer) : "0"(duration_pointer));
    if (*duration_pointer == 0) {
        return;
    }

    delay = g_open_gfx_opntex_delay;
    if (delay != 0) {
        delay--;
        g_open_gfx_opntex_delay = delay;
        if (delay == 0) {
            duration_pointer[-3] = 0x80;
            delay = duration_pointer[-0x5A6];
            control_value = duration_pointer[3];
            duration_pointer[-5] = 0;
            duration_pointer[-0x5A6] = delay | 0x80;
            duration_pointer[-4] = control_value;
        }
    }

    countdown_pointer = &g_open_script_state.opntex.frame_countdown;
    countdown = *countdown_pointer - 1;
    *countdown_pointer = countdown;
    if (countdown != 0) {
        return;
    }

    frame = g_open_script_state.opntex.first_frame;
    if (g_open_script_state.opntex.last_frame < frame) {
        g_open_script_state.opntex.frame_duration = 0;
        return;
    }

    source = g_open_gfx_opntex_data;
    source += frame * 23 * 2048;
    open_gfx_load_opntex_into_frame_buffer(source);
    g_open_script_dispatch_flags |= 0x40;
    g_open_script_state.opntex.fade_elapsed_frames = 0;
    g_open_script_state.opntex.fade_intensity = 0;
    g_open_script_state.opntex.fade_duration_frames = g_open_script_state.opntex_control_value;
    g_open_script_state.opntex.first_frame++;
    *countdown_pointer = g_open_script_state.opntex.frame_duration;
    g_open_script_state.opntex.delay
        = g_open_script_state.opntex.frame_duration - g_open_script_state.opntex_control_value;
}

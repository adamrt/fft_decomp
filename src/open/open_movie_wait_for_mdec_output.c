#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Busy-wait until the MDEC output callback has delivered the whole frame
 * (output_ready), then clear the flag. After 0x800000 passes without it the frame
 * is taken as complete and the output rectangle moves to the other frame buffer.
 * The second argument is not read. */
void open_movie_wait_for_mdec_output(open_movie_mdec_stream_state_t* state, s32 unused) {
    /* The timeout is read and written on the stack each iteration. */
    volatile s32 timeout = 0x800000;

    while (state->output_ready == 0) {
        if (--timeout == 0) {
            state->output_ready = 1;
            state->frame_rect_index = !state->frame_rect_index;
            state->output_rect.x = state->frame_rects[state->frame_rect_index].x;
            state->output_rect.y = state->frame_rects[state->frame_rect_index].y;
        }
    }

    state->output_ready = 0;
}

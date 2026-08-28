#include "fft/open.h"

void open_movie_wait_for_mdec_output(open_movie_mdec_stream_state_t* state, s32 unused) {
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

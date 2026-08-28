#include "fft/open.h"

#define OPEN_MDEC_SLICE_WIDTH 24

void open_movie_init_mdec_stream_state(
    volatile open_movie_mdec_stream_state_t* state, s16 x, s16 y, s16 width, s32 height) {
    s32 height_value;
    s32 slice_width;

    state->vlc_buffers[0] = g_open_work_buffer_0;
    state->vlc_buffers[1] = g_open_work_buffer_1;
    state->vlc_buffer_index = 0;
    state->output_buffers[0] = g_open_movie_mdec_output_buffer_0;
    /* The stack-passed height is read here through a volatile view: only a
     * volatile load is ordered against the volatile state stores (a plain
     * stack load is allowed to sink past them). */
    height_value = *(volatile s32*)&height;
    state->output_buffers[1] = g_open_movie_mdec_output_buffer_1;
    slice_width = OPEN_MDEC_SLICE_WIDTH;
    state->output_buffer_index = 0;
    state->frame_rects[0].x = x;
    state->frame_rects[0].y = y;
    state->frame_rects[1].x = width;
    state->frame_rect_index = 0;
    state->output_rect.x = x;
    state->output_rect.y = y;
    state->frame_rects[1].y = height_value;
    g_open_movie_mdec_stream_state.output_rect.w = slice_width;
    ((open_movie_mdec_stream_state_t*)state)->output_ready = 0;
}

#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/pad.h"
#include "psx/types.h"

#define OPEN_MDEC_SLICE_WIDTH 24

/* Sets the two VLC and image buffers and the two frame positions. Retail
 * stores the slice width into the global state, not through `state`. */
void open_movie_init_mdec_stream_state(open_movie_mdec_stream_state_t* state, s32 x0, s32 y0, s32 x1, s32 y1) {
    state->vlc_buffers[0] = (u32*)g_open_work_buffer_0;
    state->vlc_buffers[1] = (u32*)g_open_work_buffer_1;
    state->vlc_buffer_index = 0;
    state->output_buffers[0] = g_open_movie_mdec_output_buffer_0;
    state->output_buffers[1] = g_open_movie_mdec_output_buffer_1;
    state->output_buffer_index = 0;
    state->frame_rects[0].x = x0;
    state->frame_rects[0].y = y0;
    state->frame_rects[1].x = x1;
    state->frame_rects[1].y = y1;
    state->frame_rect_index = 0;
    state->output_rect.x = x0;
    state->output_rect.y = y0;
    g_open_movie_mdec_stream_state.output_rect.w = OPEN_MDEC_SLICE_WIDTH;
    state->output_ready = 0;
}

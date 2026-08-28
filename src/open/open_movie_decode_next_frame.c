#include "fft/open.h"

s32 open_movie_decode_next_frame(open_movie_mdec_stream_state_t* state) {
    void* frame_data;
    s32 result;

    frame_data = open_movie_acquire_next_frame(state);
    if (frame_data != 0) {
        state->vlc_buffer_index = !state->vlc_buffer_index;
        DecDCTvlc(frame_data, state->vlc_buffers[state->vlc_buffer_index]);
        StFreeRing(frame_data);
        result = 1;
    } else {
        result = 0;
    }

    return result;
}

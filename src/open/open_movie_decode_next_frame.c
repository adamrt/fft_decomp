#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Fetch the next movie frame and decode its bitstream into the other VLC buffer,
 * releasing the frame's ring sectors. Returns 0 when no frame arrived. */
s32 open_movie_decode_next_frame(open_movie_mdec_stream_state_t* state) {
    u32* frame_data;

    frame_data = open_movie_acquire_next_frame(state);
    if (frame_data == 0) {
        return 0;
    }
    state->vlc_buffer_index = !state->vlc_buffer_index;
    DecDCTvlc(frame_data, state->vlc_buffers[state->vlc_buffer_index]);
    StFreeRing(frame_data);
    return 1;
}

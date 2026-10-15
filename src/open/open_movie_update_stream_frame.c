#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Per-frame movie step while a stream plays.
 *
 * When the ring has fewer than 16 free sectors the stream is masked up to the
 * frame StGetBackloc reports and the read restarts at that location. The MDEC is
 * then started on the bitstream decoded last frame and on the first output slice,
 * the next frame's bitstream is decoded, and the routine waits for the picture to
 * be complete. The stream stops when the frame header reached end_frame (pausing
 * the CD audio) or video_end_frame (leaving it playing). */
void open_movie_update_stream_frame(void) {
    CdlLOC position;
    s16 free_sectors;
    s16 read_sectors;
    s32 words;

    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING) == 0) {
        return;
    }

    StRingStatus(&free_sectors, &read_sectors);
    if (free_sectors < 16) {
        StSetMask(1, StGetBackloc(&position), -1);
        open_movie_start_cd_stream_read(&position);
    }

    DecDCTin(g_open_movie_mdec_stream_state.vlc_buffers[g_open_movie_mdec_stream_state.vlc_buffer_index], 3);
    words = g_open_movie_mdec_stream_state.output_rect.w;
    words *= g_open_movie_mdec_stream_state.output_rect.h;
    DecDCTout(
        g_open_movie_mdec_stream_state.output_buffers[g_open_movie_mdec_stream_state.output_buffer_index], words / 2);
    open_movie_decode_next_frame(&g_open_movie_mdec_stream_state);
    open_movie_wait_for_mdec_output(&g_open_movie_mdec_stream_state, 0);

    if (g_open_movie_stream_status == OPEN_MOVIE_STREAM_STOP_AND_PAUSE_AUDIO) {
        open_movie_stop_stream(1);
    }
    if (g_open_movie_stream_status == OPEN_MOVIE_STREAM_STOP) {
        open_movie_stop_stream(0);
    }
}

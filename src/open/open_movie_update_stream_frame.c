#include "fft/open.h"

typedef struct open_movie_ring_status {
    s32 backlog;
    u8 _unused_04[4];
    s16 free_sectors;
    s16 read_sectors;
    u8 _unused_0c[4]; /* 16-byte block at sp+0x10 */
} open_movie_ring_status_t;

void open_movie_update_stream_frame(void) {
    open_movie_ring_status_t status;
    s32 words;

    if ((g_open_system_runtime_flags & 2) == 0) {
        return;
    }

    StRingStatus(&status.free_sectors, &status.read_sectors);
    if (status.free_sectors < 16) {
        StSetMask(1, StGetBacklog(&status.backlog), -1);
        open_movie_start_cd_stream_read(&status.backlog);
    }

    open_bin_decdctin(g_open_movie_mdec_stream_state.vlc_buffers[g_open_movie_mdec_stream_state.vlc_buffer_index], 3);
    words = g_open_movie_mdec_stream_state.output_rect.w;
    words *= g_open_movie_mdec_stream_state.output_rect.h;
    open_bin_decdctout(
        (u32)g_open_movie_mdec_stream_state.output_buffers[g_open_movie_mdec_stream_state.output_buffer_index],
        words / 2);
    open_movie_decode_next_frame(&g_open_movie_mdec_stream_state);
    open_movie_wait_for_mdec_output(&g_open_movie_mdec_stream_state, 0);

    if (g_open_movie_stream_status == 1) {
        open_movie_stop_stream(1);
    }
    if (g_open_movie_stream_status == 2) {
        open_movie_stop_stream(0);
    }
}

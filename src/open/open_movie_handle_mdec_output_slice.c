#include "fft/main_runtime.h"
#include "fft/open.h"

/* Uploads the decoded MDEC slice and starts decoding the next one.
 *
 * The rectangle pointer and the output-buffer array are both addressed through
 * the shared stream state, so GCC derives the buffer array (`addiu s1,s0,-0x20`)
 * from the rectangle address. The slice size is built in one variable: its
 * sign extension, product and halving then share the second argument register.
 */
void open_movie_handle_mdec_output_slice(void) {
    RECT* rect;
    s32 size;

    if (g_st_cd_interrupt_pending != 0) {
        StCdInterrupt();
        g_st_cd_interrupt_pending = 0;
    }
    rect = &g_open_movie_mdec_stream_state.output_rect;
    LoadImage(rect, g_open_movie_mdec_stream_state.output_buffers[g_open_movie_mdec_stream_state.output_buffer_index]);
    g_open_movie_mdec_stream_state.output_buffer_index = !g_open_movie_mdec_stream_state.output_buffer_index;
    rect->x += g_open_movie_mdec_stream_state.output_rect.w;
    if (rect->x < g_open_movie_mdec_stream_state.frame_rects[g_open_movie_mdec_stream_state.frame_rect_index].x
            + g_open_movie_mdec_stream_state.frame_rects[g_open_movie_mdec_stream_state.frame_rect_index].w) {
        size = g_open_movie_mdec_stream_state.output_rect.w;
        size *= g_open_movie_mdec_stream_state.output_rect.h;
        size /= 2;
        open_bin_decdctout(
            g_open_movie_mdec_stream_state.output_buffers[g_open_movie_mdec_stream_state.output_buffer_index], size);
    } else {
        g_open_movie_mdec_stream_state.output_ready = 1;
        g_open_movie_mdec_stream_state.frame_rect_index = !g_open_movie_mdec_stream_state.frame_rect_index;
        rect->x = g_open_movie_mdec_stream_state.frame_rects[g_open_movie_mdec_stream_state.frame_rect_index].x;
        g_open_movie_mdec_stream_state.output_rect.y
            = g_open_movie_mdec_stream_state.frame_rects[g_open_movie_mdec_stream_state.frame_rect_index].y;
    }
}

#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/pad.h"
#include "psx/types.h"

#define OPEN_MOVIE_ACQUIRE_MAX_ATTEMPTS 0x80

/* Wait for the next movie frame (at most 129 vertical blanks), track the
 * stream position, adopt a changed frame size (clearing the screen), and size
 * the decode rectangles: 24-bit rows take three 16-bit values per two pixels.
 * Returns the frame data, or NULL on timeout. */
u32* open_movie_acquire_next_frame(open_movie_mdec_stream_state_t* state) {
    u32* frame_data;
    StHEADER* header;
    s32 attempts;

    attempts = 0;
    /* Passing header through StGetNext's 32-bit data-pointer slot preserves native header reloads. */
    while (StGetNext(&frame_data, (u32**)&header) != 0) {
        VSync(0);
        if (attempts == OPEN_MOVIE_ACQUIRE_MAX_ATTEMPTS) {
            return 0;
        }
        attempts++;
    }

    g_open_movie_current_frame = header->frameCount;
    if (header->frameCount >= g_open_movie_end_frame) {
        g_open_movie_stream_status = OPEN_MOVIE_STREAM_STOP_AND_PAUSE_AUDIO;
    }
    if (g_open_movie_video_end_frame != -1 && header->frameCount >= g_open_movie_video_end_frame) {
        g_open_movie_stream_status = OPEN_MOVIE_STREAM_STOP;
    }

    if (g_open_movie_width != header->width || g_open_movie_height != header->height) {
        RECT clear_rect;

        clear_rect.x = 0;
        clear_rect.y = 0;
        clear_rect.w = 0x1e0;
        clear_rect.h = 0x1e0;
        ClearImage(&clear_rect, 0, 0, 0);
        g_open_movie_width = header->width;
        g_open_movie_height = header->height;
        g_open_movie_aligned_height = (header->height + 15) & 0xfff0;
    }

    state->frame_rects[0].w = state->frame_rects[1].w = g_open_movie_width * 3 / 2;
    state->frame_rects[0].h = state->frame_rects[1].h = g_open_movie_aligned_height;
    state->output_rect.h = g_open_movie_aligned_height;
    return frame_data;
}

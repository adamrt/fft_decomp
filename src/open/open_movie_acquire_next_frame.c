#include "fft/open.h"

typedef struct open_movie_frame_header {
    u8 _unused_00[8];
    u32 frame_number;
    u8 _unused_0c[4];
    u16 width;
    u16 height;
} open_movie_frame_header_t;

s32 StGetNext(void** frame_data, open_movie_frame_header_t** header);

typedef struct open_movie_acquire_locals {
    void* frame_data;
    open_movie_frame_header_t* header;
    RECT clear_rect;
} open_movie_acquire_locals_t;

#define OPEN_MOVIE_ACQUIRE_MAX_ATTEMPTS 0x80

void* open_movie_acquire_next_frame(open_movie_mdec_stream_state_t* state) {
    open_movie_acquire_locals_t locals;
    s32 attempts;

    attempts = 0;
    for (;;) {
        if (StGetNext(&locals.frame_data, &locals.header) == 0) {
            break;
        }
        VSync(0);
        if (attempts == OPEN_MOVIE_ACQUIRE_MAX_ATTEMPTS) {
            return 0;
        }
        attempts++;
    }

    g_open_movie_current_frame = locals.header->frame_number;
    if (locals.header->frame_number >= g_open_movie_first_frame) {
        g_open_movie_stream_status = 1;
    }
    if (g_open_movie_last_frame != -1 && locals.header->frame_number >= (u32)g_open_movie_last_frame) {
        g_open_movie_stream_status = 2;
    }

    if (g_open_movie_width != locals.header->width || g_open_movie_height != locals.header->height) {
        locals.clear_rect.x = 0;
        locals.clear_rect.y = 0;
        locals.clear_rect.w = 0x1e0;
        locals.clear_rect.h = 0x1e0;
        ClearImage(&locals.clear_rect, 0, 0, 0);
        g_open_movie_width = locals.header->width;
        g_open_movie_height = locals.header->height;
        g_open_movie_aligned_height = (locals.header->height + 15) & 0xfff0;
    }

    {
        /* One pin: the width must be loaded into $a0 for the retail
         * schedule (the frame pointer reload then fills its delay slot). */
        register u32 movie_width __asm__("$4") = g_open_movie_width;
        void* result = locals.frame_data;
        u32 display_width;
        u32 aligned_height;

        /* 24-bit RGB rows: three halfwords per two pixels. The halving adds
         * the sign bit and then shifts unsigned (srl), so it is not a plain
         * signed division. */
        display_width = movie_width * 3;
        /* Zero-byte fence: lets the frame pointer reload fill the width
         * load's delay slot, as in the retail schedule. */
        __asm__ volatile("");
        display_width += display_width >> 31;
        display_width >>= 1;
        aligned_height = (u16)g_open_movie_aligned_height;
        state->frame_rects[1].w = display_width;
        state->frame_rects[0].w = display_width;
        state->frame_rects[1].h = aligned_height;
        state->frame_rects[0].h = aligned_height;
        state->output_rect.h = aligned_height;
        return result;
    }
}

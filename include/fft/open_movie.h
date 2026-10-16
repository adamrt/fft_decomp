#ifndef OPEN_MOVIE_H
#define OPEN_MOVIE_H

/* OPEN.BIN movie playback: the CD stream, MDEC decoding and the opening and ending movies. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* stream state */
/* g_movie_stream_status, set as each frame header arrives. */
typedef enum movie_stream_status {
    MOVIE_STREAM_PLAYING = 0,
    MOVIE_STREAM_STOP_AND_PAUSE_AUDIO = 1, /* reached g_movie_end_frame */
    MOVIE_STREAM_STOP = 2,                 /* reached g_movie_video_end_frame; the CD audio plays on */
} movie_stream_status_e;

/* Shared VLC/MDEC state used by OPEN.BIN's movie-stream pipeline. */
typedef struct movie_mdec_state {
    u32* vlc_buffers[2];     /* 0x00; decoded run-level data in 32-bit entries */
    s32 vlc_buffer_index;    /* 0x08 */
    u32* output_buffers[2];  /* 0x0c; decoded image data in 32-bit entries */
    s32 output_buffer_index; /* 0x14 */
    RECT frame_rects[2];     /* 0x18 */
    s32 frame_rect_index;    /* 0x28 */
    RECT output_rect;        /* 0x2c */
    s32 output_ready;        /* 0x34 */
} movie_mdec_state_t;
typedef char open_movie_mdec_stream_state_size_must_be_56[(sizeof(movie_mdec_state_t) == 56) ? 1 : -1];

extern movie_stream_status_e g_movie_stream_status;
extern movie_mdec_state_t g_movie_mdec_stream_state;
extern u32 g_movie_cd_stream_ring_buffer[];
extern u32 g_movie_mdec_output_buffer_0[];
extern u32 g_movie_mdec_output_buffer_1[];
extern s32 g_movie_width;
extern s32 g_movie_height;
extern s32 g_movie_aligned_height;
extern u32 g_movie_current_frame;
extern u32 g_movie_end_frame;
extern s32 g_movie_video_end_frame; /* -1: none */

/* playback */
void movie_start_stream(s32 sector, s32 end_frame, s32 video_end_frame, s32 cd_mix_level);
void movie_stop_stream(s32 pause_cd_audio);
void movie_pause_cd_audio(void);
void title_start_logo_movie_or_menu(s32 use_alternate);
void movie_stream_fftst(s32 start_sector, s32 stream_length, s32 end_frame, s32 video_end_frame);
void movie_stream_title_demo(s32 start_sector, s32 stream_length, s32 end_frame, s32 cd_mix_level);
void movie_play_ending(void);
void ending_init(void);

#endif

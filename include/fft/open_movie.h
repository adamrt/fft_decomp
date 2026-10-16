#ifndef OPEN_MOVIE_H
#define OPEN_MOVIE_H

/* OPEN.BIN movie playback: the CD stream, MDEC decoding and the opening and ending movies. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* stream state */
/* g_open_movie_stream_status, set as each frame header arrives. */
typedef enum open_movie_stream_status {
    OPEN_MOVIE_STREAM_PLAYING = 0,
    OPEN_MOVIE_STREAM_STOP_AND_PAUSE_AUDIO = 1, /* reached g_open_movie_end_frame */
    OPEN_MOVIE_STREAM_STOP = 2,                 /* reached g_open_movie_video_end_frame; the CD audio plays on */
} open_movie_stream_status_e;

/* Shared VLC/MDEC state used by OPEN.BIN's movie-stream pipeline. */
typedef struct open_movie_mdec_stream_state {
    u32* vlc_buffers[2];     /* 0x00; decoded run-level data in 32-bit entries */
    s32 vlc_buffer_index;    /* 0x08 */
    u32* output_buffers[2];  /* 0x0c; decoded image data in 32-bit entries */
    s32 output_buffer_index; /* 0x14 */
    RECT frame_rects[2];     /* 0x18 */
    s32 frame_rect_index;    /* 0x28 */
    RECT output_rect;        /* 0x2c */
    s32 output_ready;        /* 0x34 */
} open_movie_mdec_stream_state_t;
typedef char open_movie_mdec_stream_state_size_must_be_56[(sizeof(open_movie_mdec_stream_state_t) == 56) ? 1 : -1];

extern open_movie_stream_status_e g_open_movie_stream_status;
extern open_movie_mdec_stream_state_t g_open_movie_mdec_stream_state;
extern u32 g_open_movie_cd_stream_ring_buffer[];
extern u32 g_open_movie_mdec_output_buffer_0[];
extern u32 g_open_movie_mdec_output_buffer_1[];
extern s32 g_open_movie_width;
extern s32 g_open_movie_height;
extern s32 g_open_movie_aligned_height;
extern u32 g_open_movie_current_frame;
extern u32 g_open_movie_end_frame;
extern s32 g_open_movie_video_end_frame; /* -1: none */

/* playback */
void open_movie_start_stream(s32 sector, s32 end_frame, s32 video_end_frame, s32 cd_mix_level);
void open_movie_stop_stream(s32 pause_cd_audio);
void open_movie_pause_cd_audio(void);
void open_movie_start_fftst_or_title_menu(s32 use_alternate);
void open_movie_stream_fftst_and_push_controller(
    s32 start_sector, s32 stream_length, s32 end_frame, s32 video_end_frame);
void open_movie_stream_title_demo_and_push_controller(
    s32 start_sector, s32 stream_length, s32 end_frame, s32 cd_mix_level);
void open_movie_play_end(void);
void open_system_init_ending(void);

#endif

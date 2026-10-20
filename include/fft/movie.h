#ifndef MOVIE_H
#define MOVIE_H

/* OPEN's FMV movies (opening, title demo, ending): their controllers and the CD stream and MDEC decoding pipeline. */

#include "psx/types.h"

union controller_record;

/* movie controllers */
/* Movie controller state (handlers 0 and 11): the stream the FFTST and title demo movies play. */
typedef struct movie_stream_state {
    s32 start_sector;
    s32 stream_length;
} movie_stream_state_t;

/* The movies' controller handlers (CONTROLLER_*_MOVIE). */
void movie_update_fftend(union controller_record* unused_record);
void movie_update_title_demo(union controller_record* unused_record);

/* movie streams */
void movie_stream_start(s32 sector, s32 end_frame, s32 video_end_frame, s32 cd_mix_level);
void movie_stream_stop(s32 pause_cd_audio);
void movie_cd_audio_pause(void);
void movie_logo_start(s32 start_sector, s32 stream_length, s32 end_frame, s32 video_end_frame);
void movie_title_demo_start(s32 start_sector, s32 stream_length, s32 end_frame, s32 cd_mix_level);
void movie_ending_start(void);

#endif

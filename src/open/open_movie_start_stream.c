#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Starts streaming a movie. The stream stops with the CD audio paused at end_frame, and
 * without touching the audio at video_end_frame (-1: none). */
void open_movie_start_stream(s32 sector, s32 end_frame, s32 video_end_frame, s32 cd_mix_level) {
    open_sound_set_cd_mix_and_volume(0, 1);
    suzuki_sound_set_type(suzuki_sound_get_type());
    g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN | OPEN_SYSTEM_RUNTIME_FLAG_RGB24;
    open_gfx_init_screen_environments(1);

    g_open_movie_aligned_height = 0;
    g_open_movie_width = 0;
    g_open_movie_current_frame = 0;
    g_open_movie_stream_status = OPEN_MOVIE_STREAM_PLAYING;
    g_open_movie_end_frame = end_frame;
    g_open_movie_video_end_frame = video_end_frame;
    open_movie_init_mdec_stream_state(&g_open_movie_mdec_stream_state, 0, 8, 0, 0xf8);

    for (;;) {
        open_movie_init_stream(sector, open_movie_handle_mdec_output_slice);
        while (CdRead2(0x1e0) == 0) { }
        if (open_movie_decode_next_frame(&g_open_movie_mdec_stream_state) != 0) {
            break;
        }
        main_file_reset_cd_subsystems();
    }

    open_sound_set_cd_mix_and_volume(cd_mix_level, 0x20);
    PutDrawEnv(&g_open_gfx_screen_environments[0].draw);
    PutDispEnv(&g_open_gfx_screen_environments[0].disp);
    g_open_file_current_openbk_image_id = -1;
    g_open_system_runtime_flags
        = (g_open_system_runtime_flags
              & ~(OPEN_SYSTEM_RUNTIME_FLAG_WORLD_OVERLAY_LOADED | OPEN_SYSTEM_RUNTIME_FLAG_PARTY_CHOSEN))
        | (OPEN_SYSTEM_RUNTIME_FLAG_MOVIE_STREAMING | OPEN_SYSTEM_RUNTIME_FLAG_CD_AUDIO_PLAYING);
}

#include "fft/open.h"

void open_movie_start_stream(s32 sector, s32 first_frame, s32 last_frame, s32 sound_type) {
    open_sound_set_type_and_volume(0, 1);
    main_sound_set_type(main_sound_get_type());
    g_open_system_runtime_flags |= OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN | OPEN_SYSTEM_RUNTIME_FLAG_RGB24;
    open_gfx_init_screen_environments(1);

    g_open_movie_aligned_height = 0;
    g_open_movie_width = 0;
    g_open_movie_current_frame = 0;
    g_open_movie_stream_status = 0;
    g_open_movie_first_frame = first_frame;
    g_open_movie_last_frame = last_frame;
    open_movie_init_mdec_stream_state(&g_open_movie_mdec_stream_state, 0, 8, 0, 0xf8);

    for (;;) {
        open_movie_init_stream(sector, (void*)open_movie_handle_mdec_output_slice);
        while (CdRead2(0x1e0) == 0) { }
        if (open_movie_decode_next_frame(&g_open_movie_mdec_stream_state) != 0) {
            break;
        }
        main_file_reset_cd_subsystems();
    }

    open_sound_set_type_and_volume(sound_type, 0x20);
    PutDrawEnv(&g_open_gfx_screen_environments[0].draw);
    PutDispEnv(&g_open_gfx_screen_environments[0].disp);
    g_open_file_current_openbk_image_id = -1;
    g_open_system_runtime_flags = (g_open_system_runtime_flags & ~0x180) | 6;
}

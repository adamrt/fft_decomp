#include "fft/open.h"

/* Set up the ending: reset the runtime state, save the 64x256 VRAM page at (960,
 * 256) to (960, 0), load the ending's graphics and start FFTEND.STR. */
void open_movie_init_and_play_end(void) {
    RECT source;

    main_sound_unload_scenario_music_and_tunes();
    open_system_init_runtime_state();

    source.x = 0x3c0;
    source.y = 0x100;
    source.w = 0x40;
    source.h = 0x100;
    MoveImage(&source, 0x3c0, 0);
    DrawSync(0);

    g_open_system_vsync_mode = 0;
    open_file_load_end_movie_graphics();
    open_gfx_init_screen_environments(1);
    window_init_record_pools_variant();
    open_movie_play_end();
}

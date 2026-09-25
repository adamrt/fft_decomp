#include "fft/open.h"
#include "psx/types.h"

void open_movie_init_and_stream_opening(s32 skip_movie) {
    main_sound_unload_scenario_music_and_tunes();
    open_system_init_runtime_state();
    open_file_load_main_menu_graphics();
    open_gfx_init_screen_environments(1);
    open_gfx_init_render_buffers();
    open_input_init_directional_state();
    open_movie_start_fftst_or_alternate_controller(skip_movie);
}

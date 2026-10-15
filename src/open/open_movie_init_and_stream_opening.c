#include "fft/open.h"
#include "psx/types.h"

/* Set up the title sequence: reset the runtime state, load the title graphics and
 * start the FFTST.STR logo movie, or the title menu when skip_movie is set. */
void open_movie_init_and_stream_opening(s32 skip_movie) {
    main_sound_unload_scenario_music_and_tunes();
    open_system_init_runtime_state();
    open_file_load_main_menu_graphics();
    open_gfx_init_screen_environments(1);
    window_init_record_pools_variant();
    open_input_init_directional_state();
    open_movie_start_fftst_or_title_menu(skip_movie);
}

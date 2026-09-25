#include "fft/open.h"

void open_title_start_new_game_transition(void) {
    s32 controller;
    s32 stack_padding[4];

    main_sound_set_master_volume(0, 1);
    main_sound_set_current_music_track(0x22);
    main_sound_set_master_volume(0x3fff, 5);
    open_file_load_sound_menu_textures();
    open_file_build_openbk_header(1, g_open_file_destination);
    open_file_wait_for_pending();
    open_gfx_init_transition_request(12, 3, g_open_new_game_transition_entries);
    open_gfx_start_overlay_fade_out(0x20);

    controller = g_open_current_controller_index;
    g_open_controller_stream_start[controller]._unknown_10 = 0;
    g_open_controller_handler_indices[controller] = 4;
    g_open_current_controller_index = controller + 1;
}

/* The title-menu callers enter this path for the new-game selection. */

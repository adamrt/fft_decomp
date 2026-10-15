#include "fft/open.h"

/* Begin the new-game prologue: start MAIN_MUSIC_NEW_GAME with the master volume
 * ramping up from 0, upload the prologue's text pages, read OPNBK image 1 as the
 * frame image, request the 12-entry sequence g_open_new_game_transition_entries
 * with its text in ordering-table entries 3 and 4, and push the new-game
 * controller (handler 4) at step 0 as the screen is revealed. */
void open_title_start_new_game_transition(void) {
    s32 controller;
    s32 stack_padding[4];

    suzuki_sound_set_master_volume(0, 1);
    main_sound_set_current_music_track(MAIN_MUSIC_NEW_GAME);
    suzuki_sound_set_master_volume(0x3fff, 5);
    open_file_load_sound_menu_textures();
    open_file_build_openbk_header(1, g_open_file_destination);
    file_wait_for_pending_variant();
    open_gfx_init_transition_request(12, 3, g_open_new_game_transition_entries);
    open_gfx_start_overlay_fade_out(0x20);

    controller = g_open_current_controller_index;
    g_open_controller_records_next[controller].new_game.step = 0;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_NEW_GAME_START;
    g_open_current_controller_index = controller + 1;
}

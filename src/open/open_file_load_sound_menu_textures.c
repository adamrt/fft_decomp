#include "fft/open.h"

/* Upload the two TIMs at sectors 0x43 and 0x54 of OPNTEX.BIN: the text pages at
 * VRAM (256, 256) and (320, 256) that the new-game prologue draws its lines from.
 * The only caller is open_title_start_new_game_transition. */
void open_file_load_sound_menu_textures(void) {
    main_file_load_descriptor_t* header = &g_main_file_cd_state;

    file_wait_then_build_header(header, 0x150a0, 0x11, g_open_file_destination);
    file_wait_for_pending_variant();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    file_wait_then_build_header(header, 0x150b1, 0x11, g_open_file_destination);
    file_wait_for_pending_variant();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);
}

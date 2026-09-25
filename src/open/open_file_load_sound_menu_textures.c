#include "fft/open.h"
#include "psx/types.h"

void open_file_load_sound_menu_textures(void) {
    main_file_load_descriptor_t* header = &g_open_file_header;

    open_file_wait_then_build_header(header, 0x150a0, 0x11, g_open_file_destination);
    open_file_wait_for_pending();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    open_file_wait_then_build_header(header, 0x150b1, 0x11, g_open_file_destination);
    open_file_wait_for_pending();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);
}

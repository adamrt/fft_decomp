#include "fft/open.h"
#include "psx/types.h"

void open_file_load_main_menu_graphics(void) {
    main_file_load_descriptor_t* header = &g_open_file_header;

    open_file_wait_then_build_header(header, 0x1505d, 0x11, g_open_file_destination);
    open_file_wait_for_pending();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    open_file_wait_then_build_header(header, 0x1506e, 0x11, g_open_file_destination);
    open_file_wait_for_pending();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    open_file_wait_then_build_header(header, 0x1507f, 0x21, g_open_file_destination);
    open_file_wait_for_pending();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    open_file_build_openbk_header(0, g_open_file_destination);
    open_file_wait_for_pending();
}

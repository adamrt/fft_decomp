#include "fft/open.h"

/* Load the title screen's graphics: upload the three TIMs at sectors 0, 0x11 and
 * 0x22 of OPNTEX.BIN, then read OPNBK image 0 as the frame image. */
void open_file_load_main_menu_graphics(void) {
    main_file_load_descriptor_t* header = &g_main_file_cd_state;

    file_wait_then_build_header(header, DISC_SECTOR_OPEN_OPNTEX_BIN, 0x11, g_open_file_destination);
    file_wait_for_pending_variant();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    file_wait_then_build_header(header, 0x1506e, 0x11, g_open_file_destination);
    file_wait_for_pending_variant();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    file_wait_then_build_header(header, 0x1507f, 0x21, g_open_file_destination);
    file_wait_for_pending_variant();
    open_gfx_load_opntex_into_frame_buffer(g_open_file_destination);

    open_file_build_openbk_header(0, g_open_file_destination);
    file_wait_for_pending_variant();
}

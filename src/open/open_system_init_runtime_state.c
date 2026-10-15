#include "fft/open.h"
#include "psx/types.h"

/* Reset OPEN.BIN's state on entry: running, display blanked, 320-pixel screen;
 * the frame image buffer at 0x801d0000 and the OPNTEX data buffer at 0x80140000;
 * no OPNBK image read; the overlay fade subtractive in ordering-table entry 2; no
 * buttons, no script and the first title demo movie next. */
void open_system_init_runtime_state(void) {
    g_open_system_runtime_flags = OPEN_SYSTEM_RUNTIME_FLAG_RUNNING | OPEN_SYSTEM_RUNTIME_FLAG_DISPLAY_BLANKED
        | OPEN_SYSTEM_RUNTIME_FLAG_WIDE_SCREEN;
    g_open_file_destination = (u8*)0x801d0000;
    g_open_file_current_openbk_image_id = -1;
    g_open_gfx_opntex_data = (u32*)0x80140000;
    g_open_gfx_overlay_fade.abr = 2;
    g_open_gfx_overlay_fade.ot_index = 2;
    g_open_input_previous_buttons = 0;
    g_open_input_new_button_presses = 0;
    g_input_current_buttons = 0;
    g_menu_blocked_buttons = 0;
    g_open_menu_formation_otag_index = 4;
    g_open_script_state.dispatch.flags = 0;
    g_open_title_demo_movie_index = 0;
}

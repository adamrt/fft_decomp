#include "fft/open.h"

void open_menu_start_music_test_controller(void) {
    s32 i;
    s32 four;
    s32 controller;

    open_file_build_openbk_header(1, g_open_file_destination);
    open_file_wait_for_pending();

    for (i = 0; i < 96; i++) {
        g_open_music_test_list_entries[i] = i + 0xc000;
        g_open_music_test_list_entry_flags[i] = 0;
    }

    world_script_set_event_speed(1);
    g_open_menu_formation_entry_mask = 0x160;
    /* The same 4 is stored twice; the local keeps it in a saved register
     * across the fade-out call instead of being rematerialized. */
    four = 4;
    g_open_menu_formation_otag_index = four;
    g_open_system_runtime_flags |= 0x400;
    open_gfx_start_overlay_fade_out(0x20);

    controller = g_open_current_controller_index;
    g_open_controller_stream_start[controller].stream_start = 0;
    g_open_controller_stream_start[controller].stream_length = 0;
    g_open_controller_stream_start[controller].field_08 = 0;
    g_open_controller_stream_start[controller].field_0c = four;
    g_open_controller_stream_start[controller].field_10 = 0;
    g_open_controller_handler_indices[controller] = 7;
    g_open_current_controller_index = controller + 1;
}

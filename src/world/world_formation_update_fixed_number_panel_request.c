#include "fft/world.h"
#include "psx/types.h"

void world_formation_update_fixed_number_panel_request(void) {
    s32 state = g_world_fixed_number_panel_request_state;

    if (state != 0 && state == 1) {
        g_world_menu_screen_open_request = 1;
        world_formation_reset_menu_context();
        g_world_thread_inner_subroutine_callback = (void (*)(void))main_file_load_checked_to_address;
        world_thread_call_on_main_stack(0x1BD8, 0xE800, g_world_menu_text_file_buffer);
        world_text_init_format_section_pointers(g_world_menu_text_file_buffer);
        world_menu_set_brightness(0x80, 0x80, 0x80);
        g_world_fixed_number_panel_request_state = 0;
        g_main_system_flags |= 0x01000000;
    }
}

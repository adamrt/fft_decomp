#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Serve a pending request to open the fixed number panel from the world
 * map: reset the menu context, reload and relocate the menu text file and
 * raise MAIN_SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL. */
void world_formation_update_fixed_number_panel_request(void) {
    s32 state = g_world_fixed_number_panel_request_state;

    if (state != 0 && state == 1) {
        g_world_menu_screen_open_request = 1;
        world_formation_reset_menu_context();
        g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
        thread_call_on_main_stack(DISC_SECTOR_EVENT_WORLD_LZW, 0xE800, g_world_menu_text_file_buffer);
        text_relocate_pointer_table(g_world_menu_text_file_buffer);
        world_menu_set_brightness(0x80, 0x80, 0x80);
        g_world_fixed_number_panel_request_state = 0;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_REQUEST_FIXED_NUMBER_PANEL;
    }
}

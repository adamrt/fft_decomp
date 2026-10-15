#include "fft/world.h"

/* Open the name-entry key list: identity entry ids 0..8 plus a -1 terminator. */
void world_menu_open_name_entry_key_list(void) {
    s32 count;
    s32 i;

    count = 0;
    for (i = 8; i >= 0; i--) {
        g_world_name_entry_key_ids[i] = i;
        count++;
    }
    g_world_name_entry_key_ids[count] = -1;
    world_menu_init_and_load_scrollable_list(g_world_name_entry_key_ids, g_menu_list_selected_index,
        world_text_get_resident_data(WORLD_TEXT_DATA_NAME_ENTRY_KEYBOARD), &g_world_name_entry_keyboard_script);
}

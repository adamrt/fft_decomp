#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Seed the editable name and open its keyboard and message window. */
void world_name_init_entry(const u8* name) {
    s32 i;

    g_world_name_entry_original_name = name;
    g_world_name_entry_cursor_index = 0;
    g_world_name_entry_list_cursor = 1;
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_message_box_thread);
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x21, 0xC015, 0);
    world_menu_init_scrollable_list_core(0, 0, 0);
    g_world_menu_cursor_position = 1;
    world_text_bind_section_data(WORLD_TEXT_DATA_NAME_ENTRY_KEYBOARD);
    g_text_section_pointers[WORLD_TEXT_SECTION_NAME_ENTRY_KEYS] = g_world_text_name_entry_keys_data;
    g_world_name_entry_cursor = 0;
    i = 0;
    while ((g_world_name_entry_text[i] = name[i]) != TEXT_END_WAIT_FOR_CONFIRM) {
        if (i >= 16) {
            g_world_name_entry_text[16] = TEXT_END_WAIT_FOR_CONFIRM;
            break;
        }
        i++;
    }
    world_name_apply_entry_key(WORLD_NAME_ENTRY_KEY_CURSOR_LEFT);
    g_sound_queued_effect_id = MAIN_SFX_NONE;
    g_world_name_entry_state = 0;
    world_menu_open_name_entry_key_list();
    menu_set_vsync_mode_and_event_speed(2);
}

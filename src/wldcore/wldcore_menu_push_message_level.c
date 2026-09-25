#include "fft/open.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Appends a window-list record for a text window: optionally reloads the
 * text-section pointers, starts the text-character thread on `text` (mode
 * 0x3b when system flag 0x20000 is set, else 0x33), moves the pending menu
 * result into the record, and plays sound 0x12. */
void wldcore_menu_push_message_level(s32 text, s32 reload_text) {
    s32 index;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.reload_text = reload_text;
    if (reload_text != 0) {
        world_text_save_section_pointers();
        world_text_init_format_section_pointers(g_open_text_section_offsets);
    }
    world_thread_start(2, world_text_character_handling_thread);
    if (g_main_system_flags & 0x20000) {
        world_thread_set_parameters(2, 0x3B, text, 0);
    } else {
        world_thread_set_parameters(2, 0x33, text, 0);
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.saved_menu_result = g_wldcore_menu_result;
    g_wldcore_menu_result = -1;
    wldcore_sound_play_effect(MAIN_SFX_WINDOW_OPEN);
    index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[index].message._unknown_0c = 8;
    g_wldcore_menu_stack_records_next[index].message.window_index = 0;
    g_wldcore_menu_stack_types[index + 1] = WLDCORE_MENU_LEVEL_MESSAGE;
    g_wldcore_menu_stack_depth = index + 1;
}

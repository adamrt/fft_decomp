#include "fft/open.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Appends a window-list record for a text window: optionally reloads the
 * text-section pointers, starts the text-character thread on `text` (mode
 * 0x3b when system flag 0x20000 is set, else 0x33), saves the blocked
 * buttons in the record and blocks them all, and plays sound 0x12. */
void wldcore_menu_push_message_level(s32 text, s32 reload_text) {
    s32 index;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.reload_text = reload_text;
    if (reload_text != 0) {
        text_save_section_pointers();
        text_relocate_pointer_table(g_open_text_section_offsets);
    }
    thread_start(THREAD_SCREEN_SLOT_MESSAGE, world_text_character_handling_thread);
    if (g_main_system_flags & MAIN_SYSTEM_FLAG_UNIT_PANEL_THREADS) {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x3B, text, 0);
    } else {
        thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x33, text, 0);
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.saved_blocked_buttons
        = g_wldcore_menu_blocked_buttons;
    g_wldcore_menu_blocked_buttons = -1;
    wldcore_sound_play_effect(MAIN_SFX_WINDOW_OPEN);
    index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[index].message._unknown_0c = 8;
    g_wldcore_menu_stack_records_next[index].message.window_index = 0;
    g_wldcore_menu_stack_types[index + 1] = WLDCORE_MENU_LEVEL_MESSAGE;
    g_wldcore_menu_stack_depth = index + 1;
}

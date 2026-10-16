/* Pointing the scrollable list menu at a new entry list and resetting its render state. */

#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "fft/screen.h"
#include "psx/types.h"

/* Point the scrollable list at a -1 terminated array of entry ids and their
 * name table, count the entries (a null list keeps the previous count),
 * select entry `value`, and reset the scroll and render state. The scroll
 * base is only reset when the selection starts at the top, so a list
 * reopened on a remembered entry keeps its page. */
void menu_init_scrollable_list_core(const s16* values, s32 value, u8* text_table) {
    g_screen_list_entry_ids = values;
    g_screen_list_text_table = text_table;

    if (values != 0) {
        g_menu_list_entry_count = 0;
        while (g_screen_list_entry_ids[g_menu_list_entry_count] != -1) {
            g_menu_list_entry_count++;
        }
    }

    g_menu_list_selected_index = value;
    if (value == 0) {
        g_menu_scroll_base_index = 0;
    }
    g_menu_gfx_otag_index = 0;
    g_screen_use_scroll_position = 0;
    g_menu_list_scroll_offset = 0;
    /* 0x80 per channel draws textures at their own brightness. */
    g_screen_sprite_color.r = 0x80;
    g_screen_sprite_color.g = 0x80;
    g_screen_sprite_color.b = 0x80;
    g_screen_list_layout_pending = 1;
    g_gfx_transition_frame = 0;
    g_gfx_otag_index_locked = 0;
    g_screen_prompt_active = 0;
}

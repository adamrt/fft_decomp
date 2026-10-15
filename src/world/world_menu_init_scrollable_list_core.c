#include "fft/world.h"

/* Initialize a -1 terminated scrollable list and reset its rendering state. */
void world_menu_init_scrollable_list_core(const s16* values, s32 value, u8* text_table) {
    g_world_menu_list_entry_ids = values;
    g_world_menu_list_text_table = text_table;

    if (values != 0) {
        g_world_menu_option_count = 0;
        while (g_world_menu_list_entry_ids[g_world_menu_option_count] != -1) {
            g_world_menu_option_count++;
        }
    }

    g_world_menu_cursor_position = value;
    if (value == 0) {
        g_world_menu_scroll_offset = 0;
    }
    g_menu_gfx_otag_index = 0;
    g_world_menu_use_scroll_position = 0;
    g_world_menu_scroll_pixel_offset = 0;
    g_world_menu_sprite_color.r = 0x80;
    g_world_menu_sprite_color.g = 0x80;
    g_world_menu_sprite_color.b = 0x80;
    g_world_menu_list_layout_pending = 1;
    g_gfx_transition_frame = 0;
    g_gfx_otag_index_locked = 0;
    g_world_menu_prompt_active = 0;
}

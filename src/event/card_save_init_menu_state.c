#include "fft/event_card.h"
#include "psx/types.h"

void card_save_init_menu_state(void) {
    g_card_save_list_scroll_base_y = 0x18;
    g_card_save_list_scroll_anim_offset = 0;
    g_card_save_list_top_row = 0;
    g_card_save_list_selected_slot = 0;
    g_card_save_list_cursor_index = 0;
    g_card_save_initial_prompt_timer = 0;
    g_card_save_buffer_pointer = g_card_save_buffer;
    memset(g_card_save_buffer_pointer, 0xff, 0x1e80);
}

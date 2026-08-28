#include "fft/world.h"
#include "psx/types.h"

extern void* game_malloc(s32 size);

/* Resets the memory card menu state and allocates the 0x2000-byte load
 * buffer; also used by "Open tutorial files". */
void world_card_init_menu_state(void) {
    void* buffer;

    g_world_card_list_scroll_y = 0x18;
    g_world_card_list_scroll_velocity = 0;
    g_world_card_list_first_visible_row = 0;
    g_world_card_active_slot = 0;
    g_world_card_slot_cursor_row = 0;
    g_world_card_menu_result = 0;
    g_world_card_slot_select_timer = 0;
    buffer = game_malloc(0x2000);
    g_world_load_work_buffer = (world_card_save_buffer_t*)buffer;
    memset(buffer, 0xFF, 0x1E80);
}

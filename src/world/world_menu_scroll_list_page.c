#include "fft/world.h"
#include "psx/types.h"

/* Scroll a menu list one page up (dir == -1) or down (dir == 1), as on Square + Up/Down.
 *
 * The cursor keeps its row on the page; a move that changes the first visible
 * option plays the cursor sound. */
void world_menu_scroll_list_page(s32 dir, menu_instruction_t* instruction) {
    s32 old;
    s16 delta;
    s32 unused[2]; /* 8 frame bytes no instruction uses */

    old = g_menu_scroll_base_index;
    delta = g_menu_list_selected_index - g_menu_scroll_base_index;
    if (dir == -1) {
        g_menu_scroll_base_index -= instruction->list.visible_rows;
        if (g_menu_scroll_base_index < 0) {
            g_menu_scroll_base_index = 0;
        }
    } else if (dir == 1) {
        g_menu_scroll_base_index += instruction->list.visible_rows;
        /* `dir` is 1 here. */
        if (g_menu_list_entry_count - (g_menu_scroll_base_index + dir) < instruction->list.visible_rows) {
            g_menu_scroll_base_index = g_menu_list_entry_count - instruction->list.visible_rows;
        }
    }
    g_menu_list_selected_index = g_menu_scroll_base_index;
    world_menu_load_scrollable_list_layout(instruction);
    g_menu_list_selected_index = delta + g_menu_scroll_base_index;
    if (g_menu_list_selected_index >= g_menu_list_entry_count) {
        g_menu_list_selected_index = g_menu_list_entry_count - 1;
    }
    if (old != g_menu_scroll_base_index) {
        g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
}

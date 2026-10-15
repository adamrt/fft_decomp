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

    old = g_world_menu_scroll_offset;
    delta = g_world_menu_cursor_position - g_world_menu_scroll_offset;
    if (dir == -1) {
        g_world_menu_scroll_offset -= instruction->list.visible_rows;
        if (g_world_menu_scroll_offset < 0) {
            g_world_menu_scroll_offset = 0;
        }
    } else if (dir == 1) {
        g_world_menu_scroll_offset += instruction->list.visible_rows;
        /* `dir` is 1 here. */
        if (g_world_menu_option_count - (g_world_menu_scroll_offset + dir) < instruction->list.visible_rows) {
            g_world_menu_scroll_offset = g_world_menu_option_count - instruction->list.visible_rows;
        }
    }
    g_world_menu_cursor_position = g_world_menu_scroll_offset;
    world_menu_load_scrollable_list_layout(instruction);
    g_world_menu_cursor_position = delta + g_world_menu_scroll_offset;
    if (g_world_menu_cursor_position >= g_world_menu_option_count) {
        g_world_menu_cursor_position = g_world_menu_option_count - 1;
    }
    if (old != g_world_menu_scroll_offset) {
        g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
}

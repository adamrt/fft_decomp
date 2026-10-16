/* Scrolling a list by one visible page. */

#include "fft/menu_script.h"
#include "fft/sound.h"
#include "psx/types.h"

/* Move the list viewport by one visible page.
 *
 * The selected row retains its offset within the viewport and is clamped when
 * the last page contains fewer entries.
 */
void menu_scroll_list_by_page(menu_vertical_scroll_direction_e direction, const menu_instruction_t* menu) {
    u8 unused[8];
    s32 previous_base;
    s16 base;
    s16 row_offset;
    s16 limit;
    s32 entry_count;
    s32 next;
    s16 count;

    base = g_menu_scroll_base_index;
    previous_base = base;
    row_offset = g_menu_list_selected_index - base;
    if (direction == MENU_VERTICAL_SCROLL_UP) {
        g_menu_scroll_base_index = base - menu->list.visible_rows;
        if (g_menu_scroll_base_index < 0) {
            g_menu_scroll_base_index = 0;
        }
    } else if (direction == MENU_VERTICAL_SCROLL_DOWN) {
        g_menu_scroll_base_index = menu->list.visible_rows + base;
        limit = g_menu_list_entry_count;
        entry_count = limit;
        next = g_menu_scroll_base_index + 1;
        if (entry_count - next < menu->list.visible_rows) {
            g_menu_scroll_base_index = limit - menu->list.visible_rows;
        }
    }

    g_menu_list_selected_index = g_menu_scroll_base_index;
    menu_draw_scrollable_list(menu);
    g_menu_list_selected_index = row_offset + g_menu_scroll_base_index;
    count = g_menu_list_entry_count;
    if (g_menu_list_selected_index >= count) {
        g_menu_list_selected_index = count - 1;
    }
    if (previous_base != g_menu_scroll_base_index) {
        g_sound_queued_effect_id = SOUND_SFX_CURSOR_MOVE;
    }
}

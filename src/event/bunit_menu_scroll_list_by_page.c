#include "fft/bunit.h"
#include "fft/main_sound.h"

/* Move the list viewport by one visible page.
 *
 * The selected row retains its offset within the viewport and is clamped when
 * the last page contains fewer entries.
 */
void bunit_menu_scroll_list_by_page(bunit_menu_vertical_scroll_direction_e direction, u8* menu) {
    u8 unused[0x20];
    /* Pinned: unpinned, GCC swaps previous_base and row_offset between $s1 and
     * $s0, and folds the $a2 page-bound copy the target keeps into previous_base. */
    register s32 previous_base asm("$17");
    register s32 limit asm("$6");
    u16 selected_index;
    s16 new_base;
    s32 entry_count;
    s32 row_offset;
    s32 next;

    previous_base = g_bunit_menu_scroll_base_index;
    selected_index = g_bunit_menu_list_selected_index;
    row_offset = selected_index - previous_base;
    limit = previous_base;
    if (direction == BUNIT_VERTICAL_SCROLL_UP) {
        new_base = limit - menu[6];
        g_bunit_menu_scroll_base_index = new_base;
        if (new_base < 0) {
            g_bunit_menu_scroll_base_index = 0;
        }
    } else if (direction == BUNIT_VERTICAL_SCROLL_DOWN) {
        new_base = menu[6] + limit;
        entry_count = (s16)g_bunit_menu_list_entry_count;
        limit = entry_count;
        g_bunit_menu_scroll_base_index = new_base;
        next = new_base + 1;
        /* Hides that entry_count equals limit, so the page test reads it from
         * its own register ($v1) instead of $a2. */
        __asm__("" : "=r"(entry_count) : "0"(entry_count));
        if (entry_count - next < menu[6]) {
            g_bunit_menu_scroll_base_index = limit - menu[6];
        }
    }
    /* Keeps limit ($a2) live past both arms, so the new-base subtractions
     * write $v0 instead of overwriting $a2. */
    __asm__("" : : "r"(limit));

    g_bunit_menu_list_selected_index = g_bunit_menu_scroll_base_index;
    bunit_menu_draw_scrollable_list(menu);
    g_bunit_menu_list_selected_index = row_offset + g_bunit_menu_scroll_base_index;
    if ((s16)g_bunit_menu_list_selected_index >= (s16)g_bunit_menu_list_entry_count) {
        g_bunit_menu_list_selected_index = (s16)g_bunit_menu_list_entry_count - 1;
    }
    if (previous_base != g_bunit_menu_scroll_base_index) {
        g_bunit_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
}

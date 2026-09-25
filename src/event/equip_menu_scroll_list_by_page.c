#include "fft/equip.h"
#include "fft/main_sound.h"

/* Move a scrollable list by one page while preserving its visible row. */
void equip_menu_scroll_list_by_page(s32 direction, const u8* menu) {
    u8 unused[0x20];
    /* Pin: unpinned, GCC swaps previous_base and row_offset between $s1 and $s0. */
    register s32 previous_base asm("$17");
    /* Pin: unpinned, GCC folds this $a2 copy of the page bound into previous_base. */
    register s32 limit asm("$6");
    u16 selected_index;
    s16 new_base;
    s32 entry_count;
    s32 row_offset;
    s32 next;

    previous_base = g_equip_menu_scroll_base_index;
    selected_index = g_equip_menu_selected_list_index;
    row_offset = selected_index - previous_base;
    limit = previous_base;
    if (direction == -1) {
        new_base = limit - menu[6];
        g_equip_menu_scroll_base_index = new_base;
        if (new_base < 0) {
            g_equip_menu_scroll_base_index = 0;
        }
    } else if (direction == 1) {
        new_base = menu[6] + limit;
        entry_count = g_equip_menu_list_entry_count;
        limit = entry_count;
        g_equip_menu_scroll_base_index = new_base;
        next = new_base + 1;
        /* Hides that entry_count equals limit, so the page test reads $v1, not $a2. */
        __asm__("" : "=r"(entry_count) : "0"(entry_count));
        if (entry_count - next < menu[6]) {
            g_equip_menu_scroll_base_index = limit - menu[6];
        }
    }
    /* Keeps limit live past both arms, so the subtractions write $v0, not $a2. */
    __asm__("" : : "r"(limit));

    g_equip_menu_selected_list_index = g_equip_menu_scroll_base_index;
    equip_menu_draw_scrollable_list(menu);
    g_equip_menu_selected_list_index = row_offset + g_equip_menu_scroll_base_index;
    if ((s16)g_equip_menu_selected_list_index >= g_equip_menu_list_entry_count) {
        g_equip_menu_selected_list_index = g_equip_menu_list_entry_count - 1;
    }
    if (previous_base != g_equip_menu_scroll_base_index) {
        g_equip_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
}

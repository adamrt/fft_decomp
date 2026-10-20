/* Bind scrollable menu lists and reset their cursor, drawing and scroll state. */

#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/menu_gfx.h"
#include "fft/menu_script.h"
#include "fft/menu_window.h"
#include "psx/types.h"

#if !defined(FFT_WORLD)
/* WORLD initializes its list-rendering state in a separate native body. */
/* Point the scrollable list menu at a new -1 terminated entry list and reset
 * its render state. The scroll base is reset only when the selected index
 * is 0. */
void menu_scrollable_list_core_init(const s16* entries, s32 selected_index, u8* text_table) {
    s32 first;

    g_menu_list_entries = entries;
    g_menu_list_text_table = text_table;
    if (entries != 0) {
        first = entries[0];
        g_menu_list_entry_count = 0;
        if (first != -1) {
            do {
                g_menu_list_entry_count = g_menu_list_entry_count + 1;
            } while (entries[g_menu_list_entry_count] != -1);
        }
    }
    g_menu_list_selected_index = selected_index;
    if (selected_index == 0) {
        g_menu_scroll_base_index = 0;
    }
    g_menu_text_color.r = 0x80;
    g_menu_text_color.g = 0x80;
    g_menu_text_color.b = 0x80;
    g_menu_gfx_otag_index = 0;
    g_menu_list_row_mode = 0;
    g_menu_list_scroll_offset = 0;
    g_menu_list_redraw_pending = 1;
    g_gfx_transition_frame = 0;
#    ifndef FFT_EVENT_JOBSTTS
    /* BUNIT and EQUIP reset the ordering-table lock; JOBSTTS leaves it alone. */
    g_gfx_otag_index_locked = 0;
#    endif
#    ifdef FFT_EVENT_BUNIT
    /* Only BUNIT resets the page-scroll input gate here. */
    g_input_page_scroll_disabled = 0;
#    endif
}
#endif

/* Point the scrollable list menu at a new -1 terminated entry list with the
 * given cursor row and scroll base. */
void menu_init_scrollable_list(const s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table) {
    menu_scrollable_list_core_init(entries, selected_index, text_table);
    g_menu_scroll_base_index = scroll_base_index;
}

#if defined(FFT_EVENT_EQUIP) || defined(FFT_EVENT_JOBSTTS)
/* EQUIP and JOBSTTS keep the draw-on-initialize helper beside list setup. */
/* Set up a scrollable list at a scroll base and draw it with the given menu script. */
void menu_init_and_draw_scrollable_list(
    const s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table, const menu_instruction_t* script) {
    menu_init_scrollable_list(entries, selected_index, scroll_base_index, text_table);
    menu_draw_scrollable_list(script);
}
#endif

/* Setting up a scrollable list without a scroll base and drawing it. */

#include "fft/menu_script.h"
#include "psx/types.h"

/* Set up a scrollable list without a scroll base and draw it with the given menu script. */
void menu_init_and_draw_scrollable_list_simple(
    s16* entries, s32 selected_index, u8* text_table, const menu_instruction_t* script) {
    menu_init_scrollable_list_core(entries, selected_index, text_table);
    menu_draw_scrollable_list(script);
}

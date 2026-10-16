/* Setting up a scrollable list at a scroll base and drawing it. */

#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "psx/types.h"

/* Set up a scrollable list at a scroll base and draw it with the given menu script. */
void menu_init_and_draw_scrollable_list(
    const s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table, const menu_instruction_t* script) {
    menu_init_scrollable_list(entries, selected_index, scroll_base_index, text_table);
    menu_draw_scrollable_list(script);
}

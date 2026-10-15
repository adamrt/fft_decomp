#include "fft/event_bunit.h"

void menu_init_and_draw_scrollable_list_simple(
    s16* entries, s32 selected_index, u8* text_table, const menu_instruction_t* script) {
    menu_init_scrollable_list_core(entries, selected_index, text_table);
    menu_draw_scrollable_list(script);
}

#include "fft/open.h"

#define OPEN_MENU_TEXT_MARGIN 8

/* Build and upload a window render record, drawing text combined_text_index
 * inside it at (8, 8) first unless the index is 0, and start its pop-in
 * animation. */
void window_init_text_record_variant(s32 record_index, RECT quad, s32 depth, s32 combined_text_index, u32* image) {
    /* The pair is stack-resident (0x18 / 0x1c) because it is passed by value,
     * which also lets the y store fill the beqz delay slot. */
    point32_t text;

    open_gfx_init_window_frame_record(record_index, quad, depth, (u16*)image);

    text.x = OPEN_MENU_TEXT_MARGIN;
    text.y = OPEN_MENU_TEXT_MARGIN;
    if (combined_text_index != 0) {
        window_display_text_entry_variant(record_index, combined_text_index, text, image);
    }
    open_birthday_load_window_image(record_index, (u32*)image);
    g_open_gfx_render_records_56[record_index].flags |= WORLD_RENDER_RECORD_POP_IN;
}

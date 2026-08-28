#include "fft/open.h"

#define OPEN_MENU_TEXT_MARGIN 8

void open_text_init_menu_render_record(s32 record_index, RECT quad, s32 depth, s32 combined_text_index, void* image) {
    /* The pair is stack-resident (0x18 / 0x1c) because it is passed by value,
     * which also lets the y store fill the beqz delay slot. */
    open_point32_t text;
    s32 margin;

    open_gfx_init_window_frame_record(record_index, quad, depth, image);

    margin = OPEN_MENU_TEXT_MARGIN;
    text.x = margin;
    text.y = margin;
    if (combined_text_index != 0) {
        open_birthday_draw_menu_text_entry(record_index, combined_text_index, text, image);
    }
    open_birthday_load_window_image(record_index, image);
    g_open_gfx_render_records_56[record_index].flags |= 0x100;
}

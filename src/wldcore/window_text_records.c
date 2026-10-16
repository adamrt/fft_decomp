/* Text entries of window records, shared by the opening and the world map with their own record layouts. */

#include "fft/gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_text.h"
#include "fft/wldcore.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"
#ifdef FFT_OPENING
#    include "fft/open.h"
#endif

#ifdef FFT_WLDCORE
/* `point` is passed by value: GCC spills the two 32-bit register values to their
 * home slots before narrowing them into the origin record. */
void window_display_text_entry_variant(s32 record_index, s32 text_id, point32_t point, u32* buffer) {
    glyph_blit_t origin;

    origin.rect.x = point.x;
    origin.rect.y = point.y;
    origin.stride = g_wldcore_window_render_records[record_index].bounds.w;
    menu_display_text_entry(text_id, (u8*)buffer, &origin);
}
#endif

#ifdef FFT_OPENING
/* Draw one birthday menu entry into the window's scratch image. */
void window_display_text_entry_variant(s32 record_index, s32 text_id, point32_t point, u32* buffer) {
    glyph_blit_t origin;

    origin.rect.x = point.x;
    origin.rect.y = point.y;
    origin.stride = g_open_gfx_render_records_56[record_index].tail.quad.w;
    menu_display_text_entry(text_id, buffer, &origin);
}
#endif

#ifdef FFT_WLDCORE
void wldcore_text_get_padded_dimensions(s32 value, size32_t* output) {
    s16 maximum_line_width;
    s16 line_count;

    text_measure_pixels(&maximum_line_width, &line_count, text_find_entry(value));
    output->width = (maximum_line_width + 24) & 0xfffc;
    output->height = line_count * 16 + 16;
}
#endif

#ifdef FFT_OPENING
/* Padded pixel size of the window image for a text entry: 10 pixels per column
 * plus 24, rounded down to a multiple of 4, and 16 pixels per row plus 16. The
 * WLDCORE twin wldcore_text_get_padded_dimensions measures the text in pixels
 * instead. Nothing in OPEN.BIN calls it. */
void open_text_measure_menu_image(s32 combined_text_index, size32_t* dimensions) {
    s16 columns;
    s16 rows;

    text_measure(&columns, &rows, text_find_entry(combined_text_index));
    dimensions->width = ((columns * 10) + 24) & 0xfffc;
    dimensions->height = (rows << 4) + 16;
}
#endif

#ifdef FFT_WLDCORE
/* Build and upload a render record, optionally drawing its text label first. */
void window_init_text_record_variant(s32 record_index, RECT bounds, s32 coordinate_mode, s32 text_id, u32* image) {
    point32_t text_origin;

    wldcore_window_build_render_record_image(record_index, bounds, coordinate_mode, image);
    text_origin.x = 8;
    text_origin.y = 8;
    if (text_id != 0) {
        window_display_text_entry_variant(record_index, text_id, text_origin, image);
    }
    wldcore_window_load_image_record_to_vram(record_index, image);
    g_wldcore_window_render_records[record_index].flags |= WORLD_RENDER_RECORD_POP_IN;
}
#endif

#ifdef FFT_OPENING
#    define OPEN_MENU_TEXT_MARGIN 8

/* Build and upload a window render record, drawing text combined_text_index
 * inside it at (8, 8) first unless the index is 0, and start its pop-in
 * animation. */
void window_init_text_record_variant(s32 record_index, RECT bounds, s32 coordinate_mode, s32 text_id, u32* image) {
    /* The pair is stack-resident (0x18 / 0x1c) because it is passed by value,
     * which also lets the y store fill the beqz delay slot. */
    point32_t text_origin;

    open_gfx_init_window_frame_record(record_index, bounds, coordinate_mode, (u16*)image);

    text_origin.x = OPEN_MENU_TEXT_MARGIN;
    text_origin.y = OPEN_MENU_TEXT_MARGIN;
    if (text_id != 0) {
        window_display_text_entry_variant(record_index, text_id, text_origin, image);
    }
    open_gfx_load_window_record_image(record_index, (u32*)image);
    g_open_gfx_render_records_56[record_index].flags |= WORLD_RENDER_RECORD_POP_IN;
}
#endif

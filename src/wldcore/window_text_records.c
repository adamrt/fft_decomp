/* Text entries of window records, shared by the opening and the world map with their own record layouts. */

#include "fft/gfx.h"
#include "fft/text.h"
#include "fft/window.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draw a text entry into the window image, using its record's width as the stride.
 * `point` is passed by value: GCC spills the two 32-bit register values to their
 * home slots before narrowing them into the origin record. */
void window_display_text_entry(s32 record_index, s32 text_id, point32_t point, u32* buffer) {
    glyph_blit_t origin;

    origin.rect.x = point.x;
    origin.rect.y = point.y;
    /* WLDCORE uses 52-byte render records; OPEN uses 56-byte records. */
#ifdef FFT_WLDCORE
    origin.stride = g_window_render_records[record_index].bounds.w;
#endif
#ifdef FFT_OPEN
    origin.stride = g_window_render_records[record_index].tail.quad.w;
#endif
    menu_display_text_entry(text_id, (u8*)buffer, &origin);
}

/* Padded image size for a text entry. WLDCORE measures proportional pixel widths;
 * OPEN reserves ten pixels per column. Neither program calls this routine. */
void window_measure_text_image(s32 text_id, size32_t* output) {
    s16 measured_width;
    s16 rows;

#ifdef FFT_WLDCORE
    text_measure_pixels(&measured_width, &rows, text_find_entry(text_id));
    output->width = (measured_width + 24) & 0xfffc;
#endif
#ifdef FFT_OPEN
    text_measure(&measured_width, &rows, text_find_entry(text_id));
    output->width = (measured_width * 10 + 24) & 0xfffc;
#endif
    output->height = rows * 16 + 16;
}

/* Build and upload a window image, draw the entry at (8, 8) unless its id is zero,
 * and start the record's pop-in animation. */
void window_init_text_record(s32 record_index, RECT bounds, s32 coordinate_mode, s32 text_id, u32* image) {
    /* The pair is stack-resident because it is passed by value; in OPEN this
     * also lets the y store fill the beqz delay slot. */
    point32_t text_origin;

    /* Each program's builder initializes its own record layout and VRAM mapping. */
    window_build_render_record_image(record_index, bounds, coordinate_mode, image);
    text_origin.x = 8;
    text_origin.y = 8;
    if (text_id != 0) {
        window_display_text_entry(record_index, text_id, text_origin, image);
    }
    window_load_render_record_image(record_index, image);
    g_window_render_records[record_index].flags |= DISPLAY_OBJECT_POP_IN;
}

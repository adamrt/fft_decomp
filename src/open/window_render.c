/* Build the title screen's framed text images and bind them to OPEN's window
 * records. Image coordinates select a VRAM texture page; the record also keeps
 * its screen position, palette and pop-in state. Existing VRAM images can be
 * bound without rebuilding or uploading them. */

#include "fft/screen.h"
#include "fft/text.h"
#include "fft/window.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Build the frame image and bind its screen, CLUT and VRAM coordinates.
 * Member pointers and the grouped page calculation preserve the original address arithmetic. */
void window_build_render_record_image(s32 index, RECT quad, s32 image_page_offset, u32* image) {
    point32_t* clut;
    CVECTOR* color;
    s32 half;
    s32 scaled;

    g_window_gpu_image_records[index].flags
        &= ~(DISPLAY_OBJECT_8BIT_CLUT | DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN);
    g_window_gpu_image_records[index].position.x = quad.x - 0x80;
    g_window_gpu_image_records[index].position.y = quad.y - 0x78;
    g_window_gpu_image_records[index].image_page_offset = image_page_offset;
    clut = &g_window_gpu_image_records[index].clut;
    clut->y = 0x1E0;
    clut->x = 0;
    g_window_gpu_image_records[index].bounds = quad;
    half = quad.x / 4;
    scaled = image_page_offset * 64 + 0x180;
    g_window_gpu_image_records[index].vram_position.x = half + scaled;
    g_window_gpu_image_records[index].vram_position.y = quad.y;
    color = &g_window_gpu_image_records[index].color;
    color->b = color->g = color->r = 0x80;
    g_window_gpu_image_records[index].pop_in_step = 0;
    screen_build_window_frame_image(quad.w, quad.h, (u16*)image);
}

/* Upload a window record's image to its place in VRAM.
 *
 * The record's quad is in 4-bit texels within its texture page, four to a
 * VRAM halfword, so the image goes to x = quad.x / 4 + 0x180 + 64 * image_page_offset
 * (texture page 6 + image_page_offset) with width quad.w / 4. */
void window_load_render_record_image(s32 record_index, u32* image) {
    RECT destination;
    s32 x;
    s32 image_page_offset;
    s32 scaled;

    x = g_window_gpu_image_records[record_index].bounds.x / 4;
    image_page_offset = g_window_gpu_image_records[record_index].image_page_offset;
    scaled = (image_page_offset << 6) + 0x180;
    destination.x = x + scaled;
    destination.y = g_window_gpu_image_records[record_index].bounds.y;
    destination.w = g_window_gpu_image_records[record_index].bounds.w / 4;
    destination.h = g_window_gpu_image_records[record_index].bounds.h;
    LoadImage(&destination, image);
}

/* Draw a text entry into the window image, using its record's width as the stride.
 * `point` is passed by value: GCC spills the two 32-bit register values to their
 * home slots before narrowing them into the origin record. */
void window_display_text_entry(s32 record_index, s32 text_id, point32_t point, u32* buffer) {
    glyph_blit_t origin;

    origin.rect.x = point.x;
    origin.rect.y = point.y;
    origin.stride = g_window_gpu_image_records[record_index].bounds.w;
    menu_display_text_entry(text_id, (u8*)buffer, &origin);
}

/* Padded image size, reserving ten pixels per character column. No retail caller. */
void window_measure_text_image(s32 text_id, size32_t* output) {
    s16 measured_width;
    s16 rows;

    text_measure(&measured_width, &rows, text_find_entry(text_id));
    output->width = (measured_width * 10 + 24) & 0xfffc;
    output->height = rows * 16 + 16;
}

/* Build and upload a window image, draw the entry at (8, 8) unless its id is zero,
 * and start the record's pop-in animation. */
void window_init_text_record(s32 record_index, RECT bounds, s32 image_page_offset, s32 text_id, u32* image) {
    /* The pair is stack-resident because it is passed by value; in OPEN this
     * also lets the y store fill the beqz delay slot. */
    point32_t text_origin;

    window_build_render_record_image(record_index, bounds, image_page_offset, image);
    text_origin.x = 8;
    text_origin.y = 8;
    if (text_id != 0) {
        window_display_text_entry(record_index, text_id, text_origin, image);
    }
    window_load_render_record_image(record_index, image);
    g_window_gpu_image_records[record_index].flags |= DISPLAY_OBJECT_POP_IN;
}

/* OPEN twin of window_init_vram_render_record for the 56-byte records: it keeps
 * the VRAM position instead of a texture page and also clears the 8-bit CLUT flag.
 * Nothing in OPEN.BIN calls it. */
void window_init_vram_record(
    s32 record_index, point16_t vram_position, point16_t image_size, point32_t clut_position, u32 flags) {
    RECT* quad_base;
    RECT* quad;
    CVECTOR* color;

    g_window_gpu_image_records[record_index].flags
        = (g_window_gpu_image_records[record_index].flags
              & ~(DISPLAY_OBJECT_8BIT_CLUT | DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN))
        | flags;
    g_window_gpu_image_records[record_index].clut = clut_position;
    g_window_gpu_image_records[record_index].vram_position.x = vram_position.x;
    g_window_gpu_image_records[record_index].vram_position.y = vram_position.y;
    /* Direct quad indexing changes the original record and column base selection. */
    quad_base = &g_window_gpu_image_records[0].bounds;
    quad = quad_base + record_index * (sizeof(window_gpu_image_record_t) / sizeof(RECT));
    quad->x = (vram_position.x & 0x3f) * 2;
    quad->y = vram_position.y & 0xFF;
    quad->w = image_size.x * 2;
    quad->h = image_size.y;
    color = &g_window_gpu_image_records[record_index].color;
    color->b = color->g = color->r = 0x80;
    g_window_gpu_image_records[record_index].pop_in_step = 0;
}

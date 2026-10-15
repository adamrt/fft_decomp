/* Image record building, point transform, VRAM load and text entry display. */

#include "fft/wldcore.h"
#include "psx/libgpu.h"

/* Prepare a render record and build its CPU-side window image.
 *
 * The caller adds optional text, uploads the image, and activates the record.
 *
 * bounds arrives by value in a1/a2: the record's bounds are copied from the
 * argument save slots, and the transform call receives the same struct.
 */
void wldcore_window_build_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, u32* image) {
    point32_t* clut;
    CVECTOR* color;
    point32_t transformed;

    g_wldcore_window_render_records[index].flags
        &= ~(WORLD_RENDER_RECORD_8BIT_CLUT | WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN);
    g_wldcore_window_render_records[index].base.x = bounds.x - 0x80;
    g_wldcore_window_render_records[index].base.y = bounds.y - 0x78;
    g_wldcore_window_render_records[index].coordinate_mode = coordinate_mode;
    /* Struct view of clut_x/clut_y: as plain s32 stores they could alias the
     * stacked image argument, whose load the target schedules after them. */
    clut = &g_wldcore_window_render_records[index].clut;
    clut->y = 0x1E1;
    clut->x = 0;
    g_wldcore_window_render_records[index].bounds = bounds;
    wldcore_window_transform_point_for_mode(coordinate_mode, bounds, &transformed);
    g_wldcore_window_render_records[index].tpage = GetTPage(0, 0, transformed.x, transformed.y);
    color = &g_wldcore_window_render_records[index].color;
    color->b = color->g = color->r = 0x80;
    g_wldcore_window_render_records[index].anim_counter = 0;
    world_menu_build_window_frame_image(bounds.w, bounds.h, (u16*)image);
}

/* Transform a render position into the coordinate space selected by mode.
 *
 * The callee receives the adjacent position and dimensions registers as one
 * eight-byte aggregate. Dimensions are preserved by the ABI but unused here.
 */
void wldcore_window_transform_point_for_mode(s32 mode, RECT bounds, point32_t* output) {
    switch (mode) {
    case 0:
    case 1:
        output->x = bounds.x / 4 + 0x180 + mode * 64;
        output->y = bounds.y;
        break;
    case 2:
        output->x = (bounds.x / 4) + 0x240;
        output->y = bounds.y + 0x100;
        break;
    }
}

/* Upload a render record's window image at the VRAM position its coordinate
 * mode selects. The record's bounds are passed to the transform by value. */
void wldcore_window_load_image_record_to_vram(s32 index, u32* image) {
    point32_t transformed;
    RECT rect;

    wldcore_window_transform_point_for_mode(g_wldcore_window_render_records[index].coordinate_mode,
        g_wldcore_window_render_records[index].bounds, &transformed);
    rect.x = transformed.x;
    rect.y = transformed.y;
    rect.w = g_wldcore_window_render_records[index].bounds.w / 4;
    rect.h = g_wldcore_window_render_records[index].bounds.h;
    LoadImage(&rect, image);
}

/* `point` is passed by value: GCC spills the two 32-bit register values to their
 * home slots before narrowing them into the origin record. */
void window_display_text_entry_variant(s32 index, s32 text_id, point32_t point, u32* buffer) {
    glyph_blit_t origin;

    origin.rect.x = point.x;
    origin.rect.y = point.y;
    origin.stride = g_wldcore_window_render_records[index].bounds.w;
    menu_display_text_entry(text_id, (u8*)buffer, &origin);
}

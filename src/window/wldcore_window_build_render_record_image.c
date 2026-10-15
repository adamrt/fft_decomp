#include "fft/wldcore.h"

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

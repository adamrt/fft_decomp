#include "fft/wldcore.h"

/* Prepare a render record and build its CPU-side window image.
 *
 * The caller adds optional text, uploads the image, and activates the record.
 *
 * position and dimensions are one by-value bounds struct split over a1/a2:
 * the record's bounds are copied from their adjacent argument save slots, and
 * the transform call receives the same two words as its bounds argument.
 */
void wldcore_window_build_render_record_image(
    s32 index, wldcore_xy16_t position, wldcore_xy16_t dimensions, s32 coordinate_mode, u32* image) {
    wldcore_point32_t* clut;
    u8* color;
    s32 transformed[2];

    g_wldcore_window_render_records[index].flags &= ~0x1C;
    g_wldcore_window_render_records[index].base_x = (s16)position.x - 0x80;
    g_wldcore_window_render_records[index].base_y = (s16)position.y - 0x78;
    g_wldcore_window_render_records[index].coordinate_mode = coordinate_mode;
    /* Struct view of clut_x/clut_y: as plain s32 stores they could alias the
     * stacked image argument, whose load the target schedules after them. */
    clut = (wldcore_point32_t*)&g_wldcore_window_render_records[index].clut_x;
    clut->y = 0x1E1;
    clut->x = 0;
    *(wldcore_window_render_bounds16_t*)&g_wldcore_window_render_records[index].x
        = *(wldcore_window_render_bounds16_t*)&position;
    ((void (*)(s32, wldcore_xy16_t, wldcore_xy16_t, s32*))wldcore_window_transform_point_for_mode)(
        coordinate_mode, position, dimensions, transformed);
    g_wldcore_window_render_records[index].tpage = GetTPage(0, 0, transformed[0], transformed[1]) & 0xFFFF;
    /* One byte pointer for red/green/blue; three field stores change the bytes. */
    color = &g_wldcore_window_render_records[index].red;
    color[0] = 0x80;
    color[1] = 0x80;
    color[2] = 0x80;
    g_wldcore_window_render_records[index].anim_counter = 0;
    world_menu_build_window_frame_image((s16)dimensions.x, (s16)dimensions.y, (u16*)image);
}

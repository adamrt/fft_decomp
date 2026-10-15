#include "fft/wldcore.h"
#include "psx/libgpu.h"

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

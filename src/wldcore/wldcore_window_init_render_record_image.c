#include "fft/wldcore.h"

/* Build and upload a render record, optionally drawing its text label first. */
void wldcore_window_init_render_record_image(
    s32 index, wldcore_xy16_t position, wldcore_xy16_t dimensions, s32 coordinate_mode, s32 text_id, u32* image) {
    wldcore_point32_t text_origin;

    wldcore_window_build_render_record_image(index, position, dimensions, coordinate_mode, image);
    text_origin.x = 8;
    text_origin.y = 8;
    if (text_id != 0) {
        wldcore_menu_display_text_entry(index, text_id, text_origin, image);
    }
    wldcore_window_load_image_record_to_vram(index, image);
    g_wldcore_window_render_records[index].flags |= 0x100;
}

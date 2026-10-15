#include "fft/wldcore.h"

/* Build and upload a render record, optionally drawing its text label first. */
void window_init_text_record_variant(s32 index, RECT bounds, s32 coordinate_mode, s32 text_id, u32* image) {
    point32_t text_origin;

    wldcore_window_build_render_record_image(index, bounds, coordinate_mode, image);
    text_origin.x = 8;
    text_origin.y = 8;
    if (text_id != 0) {
        window_display_text_entry_variant(index, text_id, text_origin, image);
    }
    wldcore_window_load_image_record_to_vram(index, image);
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
}

#include "fft/wldcore.h"
#include "psx/types.h"

/* Redraw the send-unit layout render record: a window frame with one
 * 16-pixel row per chosen unit name. */
void wldcore_window_draw_text_rows(wldcore_menu_send_unit_level_t* level) {
    point32_t point;
    s32 index;
    s32 i;

    index = level->list_frame.render_index;
    world_menu_build_window_frame_image(g_wldcore_window_render_records[index].bounds.w,
        g_wldcore_window_render_records[index].bounds.h, (u16*)g_wldcore_window_image_buffer);
    point.x = 8;
    point.y = 8;
    for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
        window_display_text_entry_variant(
            index, g_wldcore_proposition_send_units[i] + TEXT_ID_UNIT_NAME_BASE, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
}

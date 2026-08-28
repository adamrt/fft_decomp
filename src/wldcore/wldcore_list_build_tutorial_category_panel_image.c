#include "fft/wldcore.h"

/* Build the image of a 0x44-wide text list panel: one 16-pixel row per text id
 * (text 0xb8d9 + id), anchored at (-0x78, -0x28) in coordinate mode 0xa.
 *
 * The base coordinates are written through a point view of base_x/base_y; the
 * target addresses them from &base_x, not from the record start. */
void wldcore_list_build_tutorial_category_panel_image(wldcore_text_list_panel_t* panel) {
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t point;
    s32 index;
    wldcore_point32_t* base;
    s32 i;

    index = panel->render_index;
    bounds.position.x = 0;
    bounds.position.y = 0;
    bounds.dimensions.x = 0x44;
    bounds.dimensions.y = panel->row_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(
        index, bounds.position, bounds.dimensions, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    g_wldcore_window_render_records[index].flags |= 0x100;
    base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < panel->row_count; i++) {
        wldcore_menu_display_text_entry(index, panel->text_ids[i] + 0xB8D9, point, g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);
}

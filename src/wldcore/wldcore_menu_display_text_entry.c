#include "fft/wldcore.h"

/* `point` is passed by value: GCC spills the two 32-bit register values to their
 * home slots before narrowing them into the origin record. */
void wldcore_menu_display_text_entry(s32 index, s32 text_id, point32_t point, u32* buffer) {
    glyph_blit_t origin;

    origin.rect.x = point.x;
    origin.rect.y = point.y;
    origin.stride = g_wldcore_window_render_records[index].bounds.w;
    menu_display_text_entry(text_id, (u8*)buffer, &origin);
}

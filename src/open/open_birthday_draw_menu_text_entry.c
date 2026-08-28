#include "fft/menu.h"
#include "fft/open.h"
#include "psx/types.h"

/* Draw one birthday menu entry into the window's scratch image. */
void open_birthday_draw_menu_text_entry(
    s32 record_index, s32 combined_text_index, open_point32_t position, void* image) {
    menu_text_origin_t origin;
    s32 x = position.x;
    s32 y = position.y;

    origin.x = x;
    origin.y = y;
    origin.stride = g_open_gfx_render_records_56[record_index].tail.quad.w;
    world_menu_display_text_entry(combined_text_index, image, &origin);
}

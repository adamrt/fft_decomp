#include "fft/open.h"
#include "psx/types.h"

/* Draw one birthday menu entry into the window's scratch image. */
void open_birthday_draw_menu_text_entry(s32 record_index, s32 combined_text_index, point32_t position, u8* image) {
    glyph_blit_t origin;

    origin.rect.x = position.x;
    origin.rect.y = position.y;
    origin.stride = g_open_gfx_render_records_56[record_index].tail.quad.w;
    menu_display_text_entry(combined_text_index, image, &origin);
}

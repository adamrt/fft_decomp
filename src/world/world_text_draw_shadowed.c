#include "fft/world.h"
#include "psx/types.h"

void world_text_draw_shadowed(void* buffer, world_glyph_blit_t* dest) {
    dest->rect.x -= 2;
    dest->rect.y += 2;
    world_text_blit_glyph(g_world_menu_glyph_sheet, buffer, g_world_text_shadow_top_glyph_source, dest);

    dest->rect.y += 4;
    world_text_blit_glyph(g_world_menu_glyph_sheet, buffer, g_world_text_shadow_bottom_glyph_source, dest);
}

#include "fft/world.h"
#include "psx/types.h"

void world_gfx_set_blit_dest_position(s32 x, s32 y) {
    g_world_text_glyph_dest.rect.x = x;
    g_world_text_glyph_dest.rect.y = y;
}

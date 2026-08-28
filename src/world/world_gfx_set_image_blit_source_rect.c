#include "fft/world.h"
#include "psx/types.h"

/* Set the source rectangle used by the WORLD image blitter. */
void world_gfx_set_image_blit_source_rect(s16 x, s16 y, s16 width, s16 height) {
    g_world_gfx_image_blit_source.rect.x = x;
    g_world_gfx_image_blit_source.rect.y = y;
    g_world_gfx_image_blit_source.rect.w = width;
    g_world_gfx_image_blit_source.rect.h = height;
}

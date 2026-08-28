#include "fft/world.h"
#include "psx/types.h"

/* Set the destination origin used by the WORLD image blitter. */
void world_gfx_set_image_blit_destination_origin(s16 x, s16 y) {
    g_world_gfx_image_blit_destination.rect.x = x;
    g_world_gfx_image_blit_destination.rect.y = y;
}

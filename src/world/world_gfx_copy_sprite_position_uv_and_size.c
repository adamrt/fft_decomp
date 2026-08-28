#include "psx/gpu.h"
#include "psx/types.h"

typedef struct world_icon_source_t {
    u16 vram_x; /* 0x00: menu record text-image VRAM x; (vram_x & 0x3f) * 4 is SPRT.u0 */
    u8 vram_y;  /* 0x02: low byte of the menu record's VRAM y, copied to SPRT.v0 */
    u8 unused_03;
    u8 unused_04[4];
    u16 x; /* 0x08 */
    u16 y; /* 0x0A */
    u8 unused_0c[8];
    u16 width;  /* 0x14 */
    u16 height; /* 0x16 */
} world_icon_source_t;

/* The destination is an SPRT packet: bytes +0x0c/+0x0d are its u/v
 * coordinates. */
void world_gfx_copy_sprite_position_uv_and_size(world_icon_source_t* source, SPRT* sprite) {
    sprite->x0 = source->x;
    sprite->y0 = source->y;
    sprite->u0 = (source->vram_x & 0x3F) * 4;
    sprite->v0 = source->vram_y;
    sprite->w = source->width;
    sprite->h = source->height;
}

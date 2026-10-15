#include "fft/open.h"

/* Prepare a 56-byte render record for an image that is already in VRAM.
 *
 * vram_position is the image's VRAM position and image_size its size. The source
 * rectangle keeps x within the texture page of 64 16-bit columns and doubles x and the width
 * (4-bit texels). `clut_position` becomes the record's CLUT position and `flags` replaces
 * the 8-bit, blank and hidden bits. The WLDCORE twin
 * wldcore_window_init_vram_render_record stores a texture page id where this one
 * stores the VRAM position. Nothing in OPEN.BIN calls it. */
void open_menu_init_render_record_fields(
    s32 record_index, point16_t vram_position, point16_t image_size, point32_t clut_position, u32 flags) {
    RECT* quad_base;
    RECT* quad;
    CVECTOR* color;

    g_open_gfx_render_records_56[record_index].flags
        = (g_open_gfx_render_records_56[record_index].flags
              & ~(WORLD_RENDER_RECORD_8BIT_CLUT | WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN))
        | flags;
    g_open_gfx_render_records_56[record_index].tail.clut = clut_position;
    g_open_gfx_render_records_56[record_index].vram_x = vram_position.x;
    g_open_gfx_render_records_56[record_index].vram_y = vram_position.y;
    /* Direct quad indexing changes the original record and column base selection. */
    quad_base = &g_open_gfx_render_records_56[0].tail.quad;
    quad = quad_base + record_index * (sizeof(open_render_record_56_t) / sizeof(RECT));
    quad->x = (vram_position.x & 0x3f) * 2;
    quad->y = vram_position.y & 0xFF;
    quad->w = image_size.x * 2;
    quad->h = image_size.y;
    color = &g_open_gfx_render_records_56[record_index].color;
    color->b = color->g = color->r = 0x80;
    g_open_gfx_render_records_56[record_index].pop_in_step = 0;
}

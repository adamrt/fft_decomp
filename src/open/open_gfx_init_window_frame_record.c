#include "fft/open.h"

/* Initialise a 56-byte render record as a window frame and build its image.
 *
 * `tail` and `color` are pointers because the target materialises
 * &records[index].tail and &records[index].color as values, while every other
 * field is stored directly off the record array. `half` and `scaled` keep
 * the target's x / 4 + (64 * depth + 0x180) grouping. */
void open_gfx_init_window_frame_record(s32 index, RECT quad, s32 depth, u16* image) {
    open_render_record_56_tail_t* tail;
    CVECTOR* color;
    s32 half;
    s32 scaled;

    g_open_gfx_render_records_56[index].flags &= ~0x1C;
    g_open_gfx_render_records_56[index].x = quad.x - 0x80;
    g_open_gfx_render_records_56[index].y = quad.y - 0x78;
    g_open_gfx_render_records_56[index].depth = depth;
    tail = &g_open_gfx_render_records_56[index].tail;
    tail->clut.y = 0x1E0;
    tail->clut.x = 0;
    g_open_gfx_render_records_56[index].tail.quad = quad;
    half = quad.x / 4;
    scaled = depth * 64 + 0x180;
    g_open_gfx_render_records_56[index].vram_x = half + scaled;
    g_open_gfx_render_records_56[index].vram_y = quad.y;
    color = &g_open_gfx_render_records_56[index].color;
    color->r = 0x80;
    color->g = 0x80;
    color->b = 0x80;
    g_open_gfx_render_records_56[index].pop_in_step = 0;
    world_menu_build_window_frame_image(quad.w, quad.h, image);
}

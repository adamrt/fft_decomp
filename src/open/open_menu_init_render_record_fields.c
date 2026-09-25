#include "fft/open.h"

typedef struct open_packed_halfword_pair {
    u16 first;
    u16 second;
} open_packed_halfword_pair_t;

/* Passed by value in $a3 + the first stack slot; copied whole into the
 * record tail (the retail code homes $a3 and reloads both words). */
typedef struct open_render_record_tail_pair {
    s32 parameter;
    void* image;
} open_render_record_tail_pair_t;

void open_menu_init_render_record_fields(s32 record_index, open_packed_halfword_pair_t first_pair,
    open_packed_halfword_pair_t second_pair, open_render_record_tail_pair_t pair, u32 flags) {
    s32 offset = record_index * sizeof(open_render_record_56_t);
    u8* tail_base;
    open_render_record_tail_pair_t* tail;
    u8* quad_base;
    RECT* quad;
    u8* color_base;
    u8* color;

    g_open_gfx_render_records_56[record_index].flags
        = (g_open_gfx_render_records_56[record_index].flags & ~0x1c) | flags;
    /* The tail fields go through a local base pointer, as the retail code
     * materialises tail + constant before adding the record offset. */
    tail_base = (u8*)&g_open_gfx_render_records_56[0].tail;
    tail = (open_render_record_tail_pair_t*)(tail_base + offset);
    *tail = pair;
    g_open_gfx_render_records_56[record_index].vram_x = (s16)first_pair.first;
    g_open_gfx_render_records_56[record_index].vram_y = (s16)first_pair.second;
    quad_base = tail_base + 8;
    quad = (RECT*)(quad_base + offset);
    quad->x = (first_pair.first & 0x3f) * 2;
    quad->y = (u8)first_pair.second;
    quad->w = (s16)second_pair.first * 2;
    quad->h = second_pair.second;
    color_base = tail_base + 0x10;
    color = color_base + offset;
    color[0] = 0x80;
    color[1] = 0x80;
    color[2] = 0x80;
    g_open_gfx_render_records_56[record_index].pop_in_step = 0;
}

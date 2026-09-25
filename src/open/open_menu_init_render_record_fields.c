#include "fft/open.h"

typedef struct open_menu_packed_halfword_pair {
    u16 first;
    u16 second;
} open_menu_packed_halfword_pair_t;

/* Passed by value in $a3 + the first stack slot; copied whole into the
 * record tail (the retail code homes $a3 and reloads both words). */
typedef struct open_menu_render_record_tail_pair {
    s32 parameter;
    void* image;
} open_menu_render_record_tail_pair_t;

void open_menu_init_render_record_fields(s32 record_index, open_menu_packed_halfword_pair_t first_pair,
    open_menu_packed_halfword_pair_t second_pair, open_menu_render_record_tail_pair_t pair, u32 flags) {
    open_menu_render_record_tail_pair_t* tail_base;
    open_menu_render_record_tail_pair_t* tail;
    RECT* quad_base;
    RECT* quad;
    CVECTOR* color_base;
    CVECTOR* color;

    g_open_gfx_render_records_56[record_index].flags
        = (g_open_gfx_render_records_56[record_index].flags & ~0x1c) | flags;
    /* The retail code materialises each field base from the tail before
     * adding the record stride. */
    tail_base = (open_menu_render_record_tail_pair_t*)&g_open_gfx_render_records_56[0].tail;
    tail = tail_base + record_index * (sizeof(open_render_record_56_t) / sizeof(*tail_base));
    *tail = pair;
    g_open_gfx_render_records_56[record_index].vram_x = (s16)first_pair.first;
    g_open_gfx_render_records_56[record_index].vram_y = (s16)first_pair.second;
    quad_base = (RECT*)(tail_base + 1);
    quad = quad_base + record_index * (sizeof(open_render_record_56_t) / sizeof(RECT));
    quad->x = (first_pair.first & 0x3f) * 2;
    quad->y = (u8)first_pair.second;
    quad->w = (s16)second_pair.first * 2;
    quad->h = second_pair.second;
    color_base = (CVECTOR*)(tail_base + 2);
    color = color_base + record_index * (sizeof(open_render_record_56_t) / sizeof(CVECTOR));
    color->r = 0x80;
    color->g = 0x80;
    color->b = 0x80;
    g_open_gfx_render_records_56[record_index].pop_in_step = 0;
}

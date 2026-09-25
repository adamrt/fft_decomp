#include "fft/open.h"

/* The colour triple at +0x20 of each record is written through its own base
 * pointer: the target hoists `addiu a3,a3,0x20` out of the three byte stores
 * and re-derives the record index for each one, so the stores are
 * `sb 0/1/2(records + 0x20 + index * 36)` rather than
 * `sb 0x20/0x21/0x22(records + index * 36)`.  Same 36-byte stride as
 * open_render_record_36_t. */
typedef struct open_gfx_render_record_36_color_view {
    /* 0x00 */ u8 r;
    /* 0x01 */ u8 g;
    /* 0x02 */ u8 b;
    /* 0x03 */ u8 tail[0x21];
} open_gfx_render_record_36_color_view_t;

typedef char open_gfx_render_record_36_color_size_must_be_0x24[(sizeof(open_gfx_render_record_36_color_view_t) == 0x24)
        ? 1
        : -1];

s32 open_gfx_append_render_record_36(open_render_record_t** list, s32* count) {
    open_render_record_36_t* records = g_open_gfx_render_records_36;
    open_gfx_render_record_36_color_view_t* colors = (open_gfx_render_record_36_color_view_t*)&records[0].r;
    s32 index;

    list[*count] = (open_render_record_t*)&records[g_open_gfx_next_render_record_36];
    g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].flags &= ~0x38;
    g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].palette = 0;
    /* Without this scheduling boundary the whole function swaps v0 and v1. */
    __asm__ volatile("");
    colors[g_open_gfx_next_render_record_36].r = 0x80;
    colors[g_open_gfx_next_render_record_36].g = 0x80;
    colors[g_open_gfx_next_render_record_36].b = 0x80;
    (*count)++;
    index = g_open_gfx_next_render_record_36;
    g_open_gfx_next_render_record_36 = index + 1;
    return index;
}

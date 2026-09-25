#include "fft/open.h"

/* The colour triple at +0x34 of each record is written through its own base
 * pointer (records + 0x34), indexed by the record counter, exactly as in
 * open_gfx_append_render_record_36: the target hoists `addiu a3,a3,0x34` out
 * of the three byte stores.  Same 56-byte stride as
 * open_render_record_56_t. */
typedef struct open_gfx_render_record_56_color_view {
    /* 0x00 */ u8 r;
    /* 0x01 */ u8 g;
    /* 0x02 */ u8 b;
    /* 0x03 */ u8 tail[0x35];
} open_gfx_render_record_56_color_view_t;

typedef char open_gfx_render_record_56_color_size_must_be_0x38[(sizeof(open_gfx_render_record_56_color_view_t) == 0x38)
        ? 1
        : -1];

s32 open_gfx_append_render_record_56(open_render_record_t** list, s32* count) {
    open_render_record_56_t* records = g_open_gfx_render_records_56;
    open_gfx_render_record_56_color_view_t* colors = (open_gfx_render_record_56_color_view_t*)&records[0].color.r;
    s32 index;

    list[*count] = (open_render_record_t*)&records[g_open_gfx_render_record_56_count];
    g_open_gfx_render_records_56[g_open_gfx_render_record_56_count].flags &= ~0x3c;
    g_open_gfx_render_records_56[g_open_gfx_render_record_56_count].palette = 0;
    /* Emits nothing; without this boundary the whole function swaps v0 and v1. */
    __asm__ volatile("");
    colors[g_open_gfx_render_record_56_count].r = 0x80;
    colors[g_open_gfx_render_record_56_count].g = 0x80;
    colors[g_open_gfx_render_record_56_count].b = 0x80;
    g_open_gfx_render_records_56[g_open_gfx_render_record_56_count].pop_in_step = 0;
    (*count)++;
    index = g_open_gfx_render_record_56_count;
    g_open_gfx_render_record_56_count = index + 1;
    return index;
}

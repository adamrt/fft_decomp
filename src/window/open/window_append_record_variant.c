#include "fft/open.h"

/* Appends the next free 56-byte render record to `list`, resets it and
 * returns its pool index. */
s32 window_append_record_variant(void** list, s32* count) {
    list[*count] = &g_open_gfx_render_records_56[g_open_gfx_next_render_record_56];
    g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].flags &= ~(WORLD_RENDER_RECORD_8BIT_CLUT
        | WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_SEMI_TRANSPARENT);
    g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].palette = 0;
    /* Direct color members change the shared base and add four instructions. */
    (&g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].color)->r = 0x80;
    (&g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].color)->g = 0x80;
    (&g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].color)->b = 0x80;
    g_open_gfx_render_records_56[g_open_gfx_next_render_record_56].pop_in_step = 0;
    (*count)++;
    return g_open_gfx_next_render_record_56++;
}

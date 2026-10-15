#include "fft/open.h"

/* Appends the next free 36-byte render record to `list`, resets it and
 * returns its pool index. */
s32 window_append_animated_record_variant(void** list, s32* count) {
    list[*count] = &g_open_gfx_render_records_36[g_open_gfx_next_render_record_36];
    g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].flags
        &= ~(WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_SEMI_TRANSPARENT);
    g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].palette = 0;
    /* Direct color members change the shared base and add four instructions. */
    (&g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].color)->r = 0x80;
    (&g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].color)->g = 0x80;
    (&g_open_gfx_render_records_36[g_open_gfx_next_render_record_36].color)->b = 0x80;
    (*count)++;
    return g_open_gfx_next_render_record_36++;
}

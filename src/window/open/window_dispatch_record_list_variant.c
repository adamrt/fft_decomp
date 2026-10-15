#include "fft/open.h"

/* Draw the listed render records into the ordering table. A record with BLANK or
 * HIDDEN set is skipped; ANIMATED selects the 36-byte layout and IMAGE the
 * 56-byte one. Nothing is drawn while
 * OPEN_SYSTEM_RUNTIME_FLAG_RENDER_RECORDS_HIDDEN is set, which no code in
 * OPEN.BIN does. */
void window_dispatch_record_list_variant(GsOT* otag, void** records, s32 count) {
    s32 i;
    void* record;
    u32* flags_word;
    u32 flags;

    if ((g_open_system_runtime_flags & OPEN_SYSTEM_RUNTIME_FLAG_RENDER_RECORDS_HIDDEN) != 0) {
        return;
    }

    for (i = 0; i < count; i++) {
        record = records[i];
        flags_word = record;
        flags = *flags_word;
        /* Both layouts start with the 32-bit flags. */
        if ((flags & (WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN)) == 0) {
            if ((flags & WORLD_RENDER_RECORD_ANIMATED) != 0) {
                world_animated_render_record_t* animated = record;
                open_gfx_draw_animated_render_record(animated, otag);
            } else if ((flags & WORLD_RENDER_RECORD_IMAGE) != 0) {
                open_render_record_56_t* window = record;
                open_gfx_draw_window_record(window, otag);
            }
        }
    }
}

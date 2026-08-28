#include "fft/open.h"

void open_gfx_draw_render_record_pointer_list(u32* otag, open_render_record_t** records, s32 count) {
    s32 i;
    open_render_record_t* record;
    u32 flags;

    if ((g_open_system_runtime_flags & 8) != 0) {
        return;
    }

    for (i = 0; i < count; i++) {
        record = records[i];
        flags = record->flags;
        /* Flags bits 0 and 1 select the record layout behind the common prefix. */
        if ((flags & 0x18) == 0) {
            if ((flags & 1) != 0) {
                open_gfx_draw_render_record_36((open_sprite_actor_t*)record, otag);
            } else if ((flags & 2) != 0) {
                open_gfx_draw_render_record_56((open_render_record_56_t*)record, otag);
            }
        }
    }
}

#include "fft/wldcore.h"
#include "psx/types.h"

/* Dispatch a list of display objects to one of two handlers by object kind.
 * Objects with flags 0x18 are skipped, and the whole pass is suppressed while
 * system flag 0x400 is set. */
void window_dispatch_record_list_variant(GsOT* ot, void** list, s32 count) {
    void* object;
    s32* flags;
    s32 i;

    if (g_main_system_flags & MAIN_SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS) {
        return;
    }
    for (i = 0; i < count; i++) {
        object = list[i];
        flags = object;
        if (*flags & (WORLD_RENDER_RECORD_BLANK | WORLD_RENDER_RECORD_HIDDEN)) {
            continue;
        }
        if (*flags & WORLD_RENDER_RECORD_ANIMATED) {
            world_animated_render_record_t* animated = object;
            wldcore_gfx_draw_animated_display_object(animated, ot);
        } else if (*flags & WORLD_RENDER_RECORD_IMAGE) {
            wldcore_window_render_record_t* image = object;
            wldcore_gfx_draw_display_object_sprite(image, ot);
        }
    }
}

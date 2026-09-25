#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Dispatch a list of display objects to one of two handlers by object kind.
 * Objects with flags 0x18 are skipped, and the whole pass is suppressed while
 * system flag 0x400 is set. */
void wldcore_dispatch_display_object_list(GsOT* ot, wldcore_display_object_t** list, s32 count) {
    wldcore_display_object_t* object;
    s32 i;

    if (g_main_system_flags & 0x400) {
        return;
    }
    for (i = 0; i < count; i++) {
        object = list[i];
        if (object->flags & 0x18) {
            continue;
        }
        if (object->flags & 1) {
            wldcore_gfx_draw_animated_display_object((wldcore_anim_object_t*)object, ot);
        } else if (object->flags & 2) {
            wldcore_gfx_draw_display_object_sprite(object, ot);
        }
    }
}

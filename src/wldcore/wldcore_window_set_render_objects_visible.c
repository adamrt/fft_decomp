#include "fft/wldcore.h"
#include "psx/types.h"

/* Clears (nonzero argument) or sets flag 0x8 on every object in the auxiliary
 * and main render-object queues. The flag's meaning is not yet established. */
void wldcore_window_set_render_objects_visible(s32 clear) {
    s32 i;

    if (clear) {
        for (i = 0; i < g_wldcore_window_aux_render_object_count; i++) {
            *g_wldcore_window_aux_render_object_queue[i] &= ~8;
        }
        for (i = 0; i < g_wldcore_window_render_object_count; i++) {
            *g_wldcore_window_render_object_queue[i] &= ~8;
        }
    } else {
        for (i = 0; i < g_wldcore_window_aux_render_object_count; i++) {
            *g_wldcore_window_aux_render_object_queue[i] |= 8;
        }
        for (i = 0; i < g_wldcore_window_render_object_count; i++) {
            *g_wldcore_window_render_object_queue[i] |= 8;
        }
    }
}

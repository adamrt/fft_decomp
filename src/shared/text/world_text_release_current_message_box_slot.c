#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Free the message box slot (of three) owned by the current thread, if any. */
void world_text_release_current_message_box_slot(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (g_thread_current_id == g_world_text_message_box_slot_threads[i]) {
            g_world_text_message_box_slot_threads[i] = 0;
            g_world_text_message_box_slot_glyph_counters[i] = 0;
            return;
        }
    }
}

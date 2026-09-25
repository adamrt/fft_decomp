#include "fft/world.h"
#include "psx/types.h"

void world_thread_clear_current_slot(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (g_world_thread_current_id == g_world_text_message_box_slot_threads[i]) {
            g_world_text_message_box_slot_threads[i] = 0;
            g_world_text_message_box_slot_glyph_counters[i] = 0;
            return;
        }
    }
}

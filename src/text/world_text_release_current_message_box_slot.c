#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

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

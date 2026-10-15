#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Release the typewriter text slot the current thread owns, if any: clear its
 * owner and its glyph counter. */
void menu_release_current_thread_slot(void) {
    s32 index;
    s32 current_thread_id;
    s32* owner_thread_id;
    s32* slot_state;

    index = 0;
    current_thread_id = g_thread_current_id;
    slot_state = g_text_slot_glyph_counts;
    owner_thread_id = g_text_slot_owner_thread_ids;
    for (;;) {
        if (current_thread_id == *owner_thread_id) {
            *owner_thread_id = 0;
            *slot_state = 0;
            return;
        }
        slot_state++;
        index++;
        owner_thread_id++;
        if (index >= 3) {
            return;
        }
    }
}

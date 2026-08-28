#include "fft/thread.h"

typedef struct battle_cursor_bob_threshold {
    s8 threshold;
    s8 offset;
} battle_cursor_bob_threshold_t;

extern battle_cursor_bob_threshold_t g_battle_cursor_bob_thresholds[];
extern battle_cursor_bob_threshold_t g_battle_cursor_bob_active_thresholds[];

/* Return the cursor's vertical animation offset for the current menu frame.
 *
 * The two signed threshold tables produce the idle and active cursor curves. */
s32 battle_menu_get_cursor_bob_offset(s32 mode) {
    battle_cursor_bob_threshold_t* entry;
    s32 counter;
    s32 threshold;

    counter = g_battle_thread_contexts[g_battle_current_thread_id].task_words[0];
    if (mode == 0) {
        entry = g_battle_cursor_bob_thresholds;
        counter %= entry->threshold;
    } else if (mode == 1) {
        entry = g_battle_cursor_bob_active_thresholds;
    }

    /* Goto loop: the target emits the found exit between the loop entry
     * jump and the loop body; while/for forms place it after the mode-0
     * branch instead. */
    entry++;
    goto test_threshold;

found:
    return entry->offset;

compare_threshold:
    if (counter < threshold) {
        goto found;
    }
    entry++;

test_threshold:
    threshold = entry->threshold;
    if (threshold != 0) {
        goto compare_threshold;
    }
    return 0;
}

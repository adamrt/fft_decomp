#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Map the current thread's first parameter through a threshold table:
 * mode 0 reduces it modulo the table's first byte first, mode 1 uses the
 * second table as-is. Returns the value paired with the first threshold
 * above the parameter, or 0. */
s32 world_lookup_thread_parameter_threshold_value(s32 mode) {
    s32 value = g_world_thread_contexts[g_world_thread_current_id].task_words[0];
    s8* table;
    s32 threshold;

    /* An if/else-if chain, not a switch: the target's second test branches to
     * the join (the default edge) and falls through into the mode==1 body. A
     * switch makes both cases out-of-line and costs an extra `j`/delay pair.
     * Neither arm taken leaves `table` indeterminate, as in the original. */
    if (mode == 0) {
        table = g_world_cursor_bob_thresholds;
        value = value % table[0];
    } else if (mode == 1) {
        table = g_world_cursor_bob_active_thresholds;
    }
    table += 2;
    /* A label loop, not a `while`: structured spellings (top-test,
     * assignment-in-condition, inverted body test) place the `return table[1]`
     * block between the two arms above instead of after them. Only this layout
     * -- entry straight to the bottom test, early-return block ahead of the
     * body -- reproduces the target's block order. */
    goto test;
found:
    return table[1];
body:
    if (value < threshold) {
        goto found;
    }
    table += 2;
test:
    threshold = table[0];
    if (threshold != 0) {
        goto body;
    }
    return 0;
}

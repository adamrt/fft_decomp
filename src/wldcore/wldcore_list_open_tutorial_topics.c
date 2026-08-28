#include "fft/wldcore.h"
#include "psx/types.h"

/* Appends one entry to the panel list: its value column (g_wldcore_list_entry_values), a
 * cleared flag column (g_wldcore_list_row_flags), and the running count. The same
 * three-step append appears in wldcore_proposition_load_send_unit_candidates.
 *
 * The do/while(0) statement-macro wrapper is load-bearing: without it the
 * entry index and the two strength-reduced array pointers rotate through
 * $v1/$a2/$a0 instead of the target's $a2/$a0/$v1. The likely mechanism is
 * the wrapper's loop notes changing how loop.c treats the enclosing loop. */
#define append_panel_list_entry(count, value)                                                                          \
    do {                                                                                                               \
        g_wldcore_list_entry_values[count] = (value);                                                                  \
        g_wldcore_list_row_flags[count] = 0;                                                                           \
        (count) = (count) + 1;                                                                                         \
    } while (0)

/* Opens a 41-entry panel list (halfword values 0x8000-0x8028), shows up to
 * eight rows at (0x54, 0x50) with width 0x8c, and appends its 0x5c-byte
 * record tagged 0x21.
 *
 * The list is indexed by `count` rather than `i`; indexing by `i` lets loop.c
 * fold the final count to the constant 41. */
void wldcore_list_open_tutorial_topics(void) {
    wldcore_point32_t dimensions;
    wldcore_point32_t origin;
    s32 count;
    s32 i;
    s32 visible;
    s32 record_index;

    count = 0;
    for (i = 0; i < 41; i++) {
        append_panel_list_entry(count, i - 0x8000);
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        visible = count;
    } else {
        visible = 8;
    }
    dimensions.x = 0x8c;
    origin.x = 0x54;
    dimensions.y = visible;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(0xc, count, visible);
    wldcore_window_start_panel_render_thread(
        0xc, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &dimensions, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_result = 0x160;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_TUTORIAL_TOPICS;
    g_wldcore_menu_stack_depth = record_index + 1;
}

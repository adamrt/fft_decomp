#include "fft/script.h"
#include "fft/wldcore.h"

/* Builds the entry kinds of the list-window level that 0x80080e54 pushes.
 *
 * Kind 0 is listed when script flag 0x92 is set and any of flags 0x1a4-0x1bb
 * is; kinds 1 and 2 always; kind 3 when a proposition is active or any of
 * variables 0x360-0x3bf has bit 2 set; kind 4 when any of flags 0x350-0x35f is
 * set; kind 5 when any of flags 0x321-0x34f is set; kind 6 always. The upper
 * and lower windows sit on the kind-4 and kind-5 rows and lose flag 8 once all
 * 16 lands or the first 31 treasures are found.
 */
void wldcore_menu_build_brave_story_entries(void) {
    s32 count;
    s32 i;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 0;
    if (script_get_variable(EVENT_SCRIPT_VAR_FACTS_ENABLED) != 0) {
        count = 0;
        for (i = 0; i < EVENT_SCRIPT_VAR_FACT_COUNT; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_FACT_FIRST) != 0) {
                count++;
            }
        }
        if (count != 0) {
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[0]
                = WLDCORE_BRAVE_STORY_ROW_SCRIPT_FLAGS_01A4_01BB;
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 1;
        }
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
        .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
        = WLDCORE_BRAVE_STORY_ROW_STORY_EVENTS;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
        .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count + 1]
        = WLDCORE_BRAVE_STORY_ROW_SET_SCRIPT_VARIABLES_03C0_03FF;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count += 2;

    count = g_main_save_proposition_count;
    for (i = 0; i < EVENT_SCRIPT_VAR_PROPOSITION_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_PROPOSITION_FIRST) & WLDCORE_PROPOSITION_VARIABLE_COMPLETED) {
            count++;
        }
    }
    if (count != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = WLDCORE_BRAVE_STORY_ROW_PROPOSITIONS;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }

    count = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window]
        .flags |= WORLD_RENDER_RECORD_BLANK;
    for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST) != 0) {
            count++;
        }
    }
    if (count != 0) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window]
            .position.y
            = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0xE;
        if (count >= 0x10) {
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                         .list_window.upper_window]
                .flags &= ~WORLD_RENDER_RECORD_BLANK;
        }
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = WLDCORE_BRAVE_STORY_ROW_UNEXPLORED_LANDS;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }

    count = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.lower_window]
        .flags |= WORLD_RENDER_RECORD_BLANK;
    for (i = 0; i < EVENT_SCRIPT_VAR_TREASURE_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
            count++;
        }
    }
    if (count != 0) {
        count = 0;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.lower_window]
            .position.y
            = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0xE;
        for (i = 0; i < 0x1F; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
                count++;
            }
        }
        if (count >= 0x1F) {
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                         .list_window.lower_window]
                .flags &= ~WORLD_RENDER_RECORD_BLANK;
        }
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = WLDCORE_BRAVE_STORY_ROW_TREASURES;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
        .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
        = WLDCORE_BRAVE_STORY_ROW_LEAVE;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
}

#include "fft/wldcore.h"

/* Builds the entry kinds of the list-window level that 0x80080e54 pushes.
 *
 * Kind 0 is listed when script flag 0x92 is set and any of flags 0x1a4-0x1bb
 * is; kinds 1 and 2 always; kind 3 when a proposition is active or any of
 * variables 0x360-0x3bf has bit 2 set; kind 4 when any of flags 0x350-0x35f is
 * set; kind 5 when any of flags 0x321-0x34f is set; kind 6 always. The upper
 * and lower windows sit on the kind-4 and kind-5 rows and lose flag 8 once all
 * 16 lands or the first 31 treasures are found.
 *
 * The empty barriers keep `i++` out of each counting test's delay slot, which
 * the target leaves as a nop (as in wldcore_get_completion_milestone_rank).
 * The volatile entry_count store stops reorg filling the following `beqz`
 * delay slot with the next loop's `i = 0` from past the join. */
void wldcore_menu_build_brave_story_entries(void) {
    s32 count;
    s32 i;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 0;
    if (world_script_get_variable(EVENT_SCRIPT_VAR_FACTS_ENABLED) != 0) {
        count = 0;
        for (i = 0; i < 0x18; i++) {
            if (world_script_get_variable(i + 0x1A4) != 0) {
                count++;
                __asm__ volatile("");
            }
        }
        if (count != 0) {
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[0] = 0;
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 1;
        }
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
        .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count] = 1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
        .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count + 1]
        = 2;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count += 2;

    count = g_main_save_proposition_count;
    for (i = 0; i < 0x60; i++) {
        if (world_script_get_variable(i + 0x360) & 4) {
            count++;
            __asm__ volatile("");
        }
    }
    if (count != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = 3;
        ((volatile wldcore_menu_list_window_level_t*)&g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                .list_window)
            ->entry_count = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count + 1;
    }

    count = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window]
        .flags |= 8;
    for (i = 0; i < 0x10; i++) {
        if (world_script_get_variable(i + 0x350) != 0) {
            count++;
            __asm__ volatile("");
        }
    }
    if (count != 0) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window]
            .y = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0xE;
        if (count >= 0x10) {
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                         .list_window.upper_window]
                .flags &= ~8;
        }
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = 4;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }

    count = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.lower_window]
        .flags |= 8;
    for (i = 0; i < 0x2F; i++) {
        if (world_script_get_variable(i + 0x321) != 0) {
            count++;
            __asm__ volatile("");
        }
    }
    if (count != 0) {
        count = 0;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.lower_window]
            .y = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0xE;
        for (i = 0; i < 0x1F; i++) {
            if (world_script_get_variable(i + 0x321) != 0) {
                count++;
                __asm__ volatile("");
            }
        }
        if (count >= 0x1F) {
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                         .list_window.lower_window]
                .flags &= ~8;
        }
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = 5;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
        .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count] = 6;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
}

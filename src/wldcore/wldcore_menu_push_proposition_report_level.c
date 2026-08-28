#include "fft/wldcore.h"

/* Hides the parent level's two windows and render record, then pushes a
 * type-0x10 menu level for argument and sets WORLD thread 14's parameters.
 *
 * The subscripted lvalues keep both scalars in-struct for GCC's alias
 * classification, which holds the g_wldcore_job_selection.gate load and the ordering-table store
 * after the menu-record stores as in the target; plain scalars are hoisted. */
void wldcore_menu_push_proposition_report_level(s32 argument) {
    s32 depth;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window].flags
        |= 0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window].flags
        |= 0x10;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index].flags
        |= 0x10;
    g_wldcore_menu_stack_records_next[depth].proposition_report.state = -1;
    g_wldcore_menu_stack_records_next[depth].proposition_report.proposition = argument;
    g_wldcore_menu_stack_records_next[depth].proposition_report.phase = 0;
    g_wldcore_menu_stack_records_next[depth].proposition_report.load_phase = 1;
    g_wldcore_menu_stack_records_next[depth].proposition_report.render_index = -1;
    (&g_wldcore_menu_ordering_table_offset)[0] = 9;
    if (g_wldcore_job_selection.gate != 0) {
        g_wldcore_menu_stack_records_next[depth].proposition_report.state = 0x63;
    } else {
        wldcore_sound_enqueue_music_start(0x223);
    }
    world_thread_set_parameters(0xE, 0, -1, 0);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_REPORT;
    g_wldcore_menu_stack_depth++;
}

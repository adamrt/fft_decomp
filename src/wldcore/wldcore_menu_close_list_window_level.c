#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/* Clears a list level's window states, restores its hidden records, and
 * queues WORLD thread 14 with message 0xb848.
 *
 * The initial volatile reads preserve the target's content/main/side load
 * order before the record stores. The `$4` pin preserves the side-window
 * index register, and the empty `records` launder keeps the render-record
 * base in a register shared by the palette stores instead of %lo-folded
 * addresses.
 * `scratch` retains the target's 12 reserved frame bytes without instructions.
 * The chained clear shares the main-window index between both stores. */
void wldcore_menu_close_list_window_level(wldcore_menu_stack_record_t* level) {
    volatile wldcore_menu_stack_record_t* observed_level = level;
    s32 content_render = observed_level->list_window.content_render;
    s32 main_window = observed_level->list_window.main_window;
    register s32 side_window __asm__("$4") = observed_level->list_window.side_window;
    s32 scratch[3];
    s32 lower_window;
    s32 upper_window;
    s32 frame_render = level->list_window.frame_render;
    wldcore_window_render_record_t* records = g_wldcore_window_render_records;

    __asm__("" : "=r"(records) : "0"(records));
    records[content_render].palette = 0;
    records[frame_render].palette = 0;
    g_wldcore_window_records[side_window].palette = 0;
    g_wldcore_window_records[main_window].palette = 0;
    lower_window = level->list_window.lower_window;
    upper_window = level->list_window.upper_window;
    g_wldcore_window_records[lower_window].palette = 0;
    g_wldcore_window_records[upper_window].palette = 0;
    g_wldcore_window_records[level->list_window.main_window].sequence = 2;
    g_wldcore_window_records[level->list_window.main_window].frame_index
        = g_wldcore_window_records[level->list_window.main_window].anim_counter = 0;
    world_thread_set_parameters(14, 0x19, 0xB848, 0);
    if (g_wldcore_window_render_records[level->list_window.frame_render].flags & 0x10) {
        g_wldcore_window_records[level->list_window.main_window].flags &= ~0x10;
        g_wldcore_window_records[level->list_window.side_window].flags &= ~0x10;
        g_wldcore_window_render_records[level->list_window.frame_render].flags &= ~0x10;
        g_wldcore_window_render_records[level->list_window.frame_render].flags |= 0x100;
        g_wldcore_window_records[level->list_window.upper_window].flags &= ~0x10;
        g_wldcore_window_records[level->list_window.lower_window].flags &= ~0x10;
    }
    if (g_wldcore_window_render_records[level->list_window.content_render].flags & 0x10) {
        g_wldcore_window_render_records[level->list_window.content_render].flags &= ~0x10;
        g_main_system_flags &= ~0x800;
        g_wldcore_window_render_records[level->list_window.content_render].flags |= 0x100;
    }
    level->list_window.mode = 0;
}

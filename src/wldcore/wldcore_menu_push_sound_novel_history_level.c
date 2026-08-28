#include "fft/thread.h"
#include "fft/wldcore.h"

/* Provisional: position view of the 36-byte window records at +0x18. */
typedef struct wldcore_window_position36 {
    s32 x;
    s32 y;
    u8 rest[0x1c];
} wldcore_window_position36_t;

/* Pushes the save-slot selection level (menu type 0x17).
 *
 * Suspends WORLD thread 14, appends the two header windows (kinds 0x6c and
 * 0x6d, the second hidden) and the 0xf8 x 0xb0 panel render record built from
 * g_wldcore_window_image_buffer, centres the newest history entry's name text
 * (0x8800 + g_wldcore_active_saved_record.text_history_3) in it, and copies the four operand-history entries
 * (newest first) into the level at +0x0c..+0x18. It also tints the picture
 * render record to half brightness, clears the screen dim and sets the menu
 * result to -1. The first header window is hidden when the second history
 * entry is empty.
 *
 * The level is written through the sound_novel_history member of the
 * stack-record union (as in wldcore_menu_push_sound_novel_quit_level).
 *
 * Several spellings are load-bearing. `windows` holds the record base in a
 * saved register: the window positions are derived from it and the second
 * window's anim_counter store goes through it, while the other field stores use
 * the absolute `$at` form. The render position and colour pointers reproduce
 * the target's shared render-record base. The history is read through a
 * pointer to its newest entry: the indexed reads count as in-struct
 * references, so the scheduler keeps each load after the preceding level
 * store. `top` and `depth` are separate locals for the target's register
 * assignment, and `unused` preserves the 0x50-byte frame. */
void wldcore_menu_push_sound_novel_history_level(void) {
    wldcore_window_render_bounds16_t bounds;
    u8 unused[8];
    wldcore_window_record_t* windows;
    wldcore_window_position36_t* positions;
    wldcore_point32_t* position;
    wldcore_window_entry_52_rgb_t* colors;
    s16* newest;
    s32 index;
    s32 depth;
    s32 top;

    world_thread_suspend(0xE);

    windows = g_wldcore_window_records;
    positions = (wldcore_window_position36_t*)&windows->x;
    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_history.upper_window = index;
    g_wldcore_window_records[index].sequence = 0x6C;
    g_wldcore_window_records[index].priority = 9;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    positions[index].x = 0;
    positions[index].y = -0x6A;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_history.lower_window = index;
    g_wldcore_window_records[index].sequence = 0x6D;
    g_wldcore_window_records[index].priority = 9;
    windows[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_window_records[index].flags |= 0x10;
    positions[index].x = 0;
    positions[index].y = 0x5E;

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_history.render_index = index;
    bounds.dimensions.x = 0xF8;
    bounds.dimensions.y = 0xB0;
    bounds.position.x = 0;
    bounds.position.y = 0;
    wldcore_window_build_render_record_image(
        index, bounds.position, bounds.dimensions, 1, g_wldcore_window_image_buffer);
    position = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    newest = (s16*)&g_wldcore_active_saved_record.text_history_3;
    g_wldcore_window_render_records[index].priority = g_wldcore_menu_ordering_table_offset;
    position->x = -0x80;
    position->y = -0x64;
    g_wldcore_window_render_records[index].clut_x = 0x3C0;
    g_wldcore_window_render_records[index].clut_y = 0x1F2;
    wldcore_window_draw_centered_text(index, *newest + 0x8800);

    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_pair_render.first_window]
        .palette = 0xA;
    colors = (wldcore_window_entry_52_rgb_t*)&g_wldcore_window_render_records->red;
    colors[g_wldcore_active_saved_record.picture_render_index].red = 0x80;
    colors[g_wldcore_active_saved_record.picture_render_index].green = 0x80;
    colors[g_wldcore_active_saved_record.picture_render_index].blue = 0x80;
    g_wldcore_screen_fade_state.boxes[0].r = 0;
    g_wldcore_screen_fade_state.boxes[0].g = 0;
    g_wldcore_screen_fade_state.boxes[0].b = 0;
    g_wldcore_screen_fade_state.flags[0] |= 8;

    top = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.scroll = 0;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.newest_text_id = *newest;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[0] = newest[-1];
    g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[1] = newest[-2];
    g_wldcore_menu_result = -1;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[2] = newest[-3];
    if (g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[0] == -1) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[top].sound_novel_history.upper_window].flags |= 0x10;
    }

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].sound_novel_history.phase = 1;
    g_wldcore_menu_stack_records_next[depth].sound_novel_history.step = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL_HISTORY;
    g_wldcore_menu_stack_depth = depth + 1;
}

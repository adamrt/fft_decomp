#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"

/* Opens the selected rumor in a scrollable text window and hides the parent
 * level's two windows and render record until the detail level closes.
 *
 * `unused` retains the target's 8 reserved frame bytes. */
void wldcore_bar_push_rumor_detail_level(s32 value) {
    wldcore_text_scrollable_window_t* window;
    s32* anchor;
    s32 depth;
    s32 rows;
    s32 width;
    u8 unused[8];

    g_world_text_substitution_values[0] = value + 0xC800;
    world_thread_set_parameters(0xE, 0x19, 0xB809, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].rumor_detail.rumor_index = value;

    /* The target keeps the text_id address in a1 and uses signed field
     * displacements. The opaque anchor stops cse from folding the stores back
     * to absolute addresses; a direct struct-base pointer breaks the match. */
    anchor = &g_wldcore_scrollable_text_window.text_id;
    __asm__("" : "=r"(anchor) : "0"(anchor));
    window = (wldcore_text_scrollable_window_t*)(anchor - 13);

    window->text_id = value + 0x8800;
    window->image_x = 0;
    g_wldcore_scrollable_text_window.image_y = 0x60;
    window->base.x = -122;
    window->base.y = -40;
    width = 0xDC;
    rows = 8;
    /* Loads rows (8, kept in s0 for the level type) before the text_width store. */
    __asm__ volatile("" : : "r"(width), "r"(rows) : "memory");
    window->text_width = width;
    window->priority = 9;
    window->rows_per_page = rows;
    window->image_coordinate_mode = 0;
    window->extra_render_index = -1;
    window->text_substitutions[1] = -1;
    window->text_substitutions[0] = -1;
    wldcore_text_init_scrollable_window(window);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window].flags
        |= 0x10;
    g_wldcore_menu_stack_records_next[depth].rumor_detail.first_window
        = g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window].flags
        |= 0x10;
    g_wldcore_menu_stack_records_next[depth].rumor_detail.second_window
        = g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index].flags
        |= 0x10;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_menu_stack_records_next[depth].rumor_detail.render_index
        = g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_RUMOR_DETAIL;
}

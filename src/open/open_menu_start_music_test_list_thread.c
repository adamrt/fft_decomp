#include "fft/open.h"
#include "psx/types.h"

void open_menu_start_music_test_list_thread(
    s32 thread_id, s32 limit, const open_point32_t* extent, const open_point32_t* origin) {
    world_menu_text_layout_t* layout = &g_open_music_test_list_state;
    world_menu_entry_t* params = &g_open_music_test_list_params;
    s32 width;
    s32 height;
    s32 height2;
    s32 bottom;
    void (*entry)(void);
    s32 hundred;
    s32 one;
    s32 ox;
    s32 oy;
    s32 ey;

    width = extent->x;
    height = extent->y;
    layout->row_count = height;
    height2 = extent->y;
    g_open_music_test_list_state.x[1] = 4;
    g_open_music_test_list_state.x[0] = 4;
    g_open_music_test_list_state.mode[1] = 2;
    g_open_music_test_list_state.mode[2] = 2;
    g_open_music_test_list_state.text_ids[0] = g_open_music_test_list_entries;
    g_open_music_test_list_state.text_colors[0] = g_open_music_test_list_entry_flags;
    g_open_music_test_list_state.mode[0] = 0;
    params->vram_x = 0x200;
    /* The empty asms below each hold one piece of the target's schedule. This
     * boundary keeps the width load in $v1 rather than $t1. */
    __asm__ volatile("");
    hundred = 0x100;
    /* Materialises `li v0,0x100` ahead of the width add. */
    __asm__ volatile("" : "=r"(hundred) : "0"(hundred));
    width = (width + 0x18) & 0xFFFC;
    g_open_music_test_list_params.vram_y = hundred;
    g_open_music_test_list_params.inner_width = width;
    g_open_music_test_list_params.inner_height = 0;
    g_open_music_test_list_state.hidden_rows = limit - height2;
    ox = origin->x;
    g_open_music_test_list_params.window_x = ox - 0x80;
    oy = origin->y;
    entry = world_build_at_list_2;
    bottom = oy - 0x78;
    g_open_music_test_list_params.window_width = width;
    g_open_music_test_list_params.window_height = 0;
    g_open_music_test_list_params.unknown_10[0] = 0;
    g_open_music_test_list_params.unknown_10[1] = 0;
    g_open_music_test_list_params.overall_width = width;
    g_open_music_test_list_params.overall_height = 0;
    g_open_music_test_list_params.unknown_18[0] = 0;
    g_open_music_test_list_params.unknown_18[1] = 0;
    g_open_music_test_list_params.text_id = 0;
    g_open_music_test_list_params.window_y = bottom;
    ey = extent->y;
    g_open_music_test_list_params.field_0x20 = 0;
    g_open_music_test_list_params.parent_indices = g_open_music_test_row_actions;
    /* Keeps the overall_width store below the zero stores ahead of it. */
    __asm__ volatile("");
    one = 1;
    /* Keeps the row-actions address load early instead of beside its store. */
    __asm__ volatile("" : "=r"(one) : "0"(one));
    g_open_music_test_list_params.max_row_index = ey;
    /* Keeps the max_row_index store ahead of the header_id store. */
    __asm__ volatile("");
    g_open_music_test_list_params.header_id = one;
    g_open_music_test_list_params.text_binding
        = (struct world_menu_text_binding*)layout; /* a column layout, not a binding */
    g_open_music_test_list_params.value = &g_open_music_test_selected_option;
    g_open_music_test_list_params.selected_index = 0;
    g_open_music_test_list_params.select_text_table = 0;

    world_thread_start(thread_id, entry);
    world_thread_set_parameters(thread_id, (s32)params, 0, 0);
}

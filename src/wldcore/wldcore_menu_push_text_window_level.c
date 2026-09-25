#include "fft/open.h"
#include "fft/wldcore.h"

/* Pushes the self-sizing message window as menu level type 2, the windowed
 * sibling of wldcore_menu_push_message_level (0x8006e860, type 3). It stores
 * the reload flag and, when set, re-points the text formatter at
 * g_open_text_section_offsets; appends a window record (sequence 0xe,
 * priority 1); measures the entry, clamped to three rows; fills the shared
 * scrollable window and centres it and the window record on the measured box;
 * then saves g_wldcore_menu_result in the level, sets it to -1, plays sound
 * 0x12, writes the level type and bumps the depth.
 *
 * Taking the record base into `records` at the last block-one store lets CSE
 * reuse that register after the measure call, which gives the base
 * priority over `text` in global allocation (s1/s2 as in the target). The
 * combined-away address pseudo of that store keeps a reload stack slot, part
 * of the target's 40-byte locals area. `box` is an aggregate so its words stay
 * in memory across wldcore_text_init_scrollable_window; `dims` is an array so
 * it is laid out in declaration order at 32(sp). `unused_point` and `unused`
 * are sized from the target frame only; their original types are unknown. */
void wldcore_menu_push_text_window_level(s32 text, s32 reload_text) {
    wldcore_text_scrollable_window_t* window;
    wldcore_window_record_t* records;
    wldcore_point32_t* positions;
    wldcore_point32_t* position;
    s32* anchor;
    s32 index;
    wldcore_point32_t box;
    wldcore_point32_t unused_point;
    s16 dims[2];
    s32 unused[2];
    u8* entry;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.reload_text = reload_text;
    if (reload_text != 0) {
        world_text_save_section_pointers();
        world_text_init_format_section_pointers(g_open_text_section_offsets);
    }

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.window_index = index;
    g_wldcore_window_records[index].sequence = 0xE;
    g_wldcore_window_records[index].priority = 1;
    g_wldcore_window_records[index].anim_counter = 0;
    records = g_wldcore_window_records;
    records[index].frame_index = 0;

    entry = world_text_find_entry(text);
    world_text_measure_pixels(&dims[0], &dims[1], entry);
    if (dims[1] > 3) {
        dims[1] = 3;
    }

    anchor = &g_wldcore_scrollable_text_window.text_id;
    window = (wldcore_text_scrollable_window_t*)(anchor - 13);

    window->text_id = text;
    window->image_x = 0;
    g_wldcore_scrollable_text_window.image_y = 0xC0;
    box.x = (dims[0] + 24) & 0xFFFC;
    box.y = (dims[1] << 4) + 16;
    g_wldcore_scrollable_text_window.base.x = -(s32)((u32)box.x >> 1);
    g_wldcore_scrollable_text_window.base.y = -(box.y >> 1);
    g_wldcore_scrollable_text_window.text_width = dims[0];
    g_wldcore_scrollable_text_window.rows_per_page = dims[1];
    g_wldcore_scrollable_text_window.priority = 1;
    g_wldcore_scrollable_text_window.image_coordinate_mode = 0;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = -1;
    wldcore_text_init_scrollable_window(window);

    index = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.window_index;
    positions = (wldcore_point32_t*)&records[0].x;
    /* This byte-view stride retains the target addiu of &records[0].x;
     * &records[index].x removes it and breaks the exact match. */
    position = (wldcore_point32_t*)((u8*)positions + index * sizeof(wldcore_window_record_t));
    position->x = -(box.x / 2) + 2;
    position->y = -(box.y / 2) - 2;
    wldcore_sound_play_effect(MAIN_SFX_WINDOW_OPEN);

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].message.saved_menu_result = g_wldcore_menu_result;
    g_wldcore_menu_result = -1;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_TEXT_WINDOW;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}

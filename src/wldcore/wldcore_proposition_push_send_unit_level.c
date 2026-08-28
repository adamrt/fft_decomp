#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/gpu.h"

void world_build_at_list_2(void);

/* Scalar view of g_wldcore_window_panel_render_state.select_text_table: as a
 * struct field its address would share the selected_index base register, but
 * the target stores it by symbol. */
extern s16 g_wldcore_menu_scroll_offset;

/* Push the proposition send-unit menu level (type 0x24).
 *
 * Builds the available-unit list into the new level record, starts WORLD
 * thread 12 on the list-panel renderer with the shared panel render state,
 * appends the level's seven window records and five render records, and asks
 * thread 14 for message 0xb812.
 *
 * The selected index is written through the panel render state (+0x38):
 * the target derives the thread parameter from that field's address. Each
 * record's x/y pair is written through a block-local pointer so the pointer
 * shares the index product's register, and the chained frame_index/anim_counter
 * store keeps anim_counter's base in a register as in the target. */
void wldcore_proposition_push_send_unit_level(void) {
    wldcore_point32_t dimensions;
    wldcore_point32_t origin;
    wldcore_window_render_bounds16_t bounds;
    s32 index;
    s32 depth;
    s32 count;

    g_wldcore_menu_scroll_offset = 0;
    g_wldcore_window_panel_render_state.selected_index = 0;
    g_wldcore_proposition_send_unit_count = 0;
    count = wldcore_proposition_load_send_unit_candidates(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit);
    dimensions.x = 0x6E;
    dimensions.y = count;
    origin.x = 8;
    origin.y = 0x50;
    wldcore_window_init_panel_render_state(
        0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.unit_count, &dimensions, &origin);
    g_wldcore_window_panel_label_state.mode[1] = 3;
    g_wldcore_window_panel_label_state.x[1] = 0x58;
    g_wldcore_window_panel_label_state.text_ids[1] = g_wldcore_proposition_candidate_levels;
    g_wldcore_window_panel_label_state.text_colors[1] = g_wldcore_list_row_flags;
    g_wldcore_window_panel_label_state.x[0] = 0;
    g_wldcore_window_panel_render_state.header_id = 0x10;
    g_wldcore_window_panel_render_state.selected_index = 0;
    g_wldcore_menu_scroll_offset = 0;
    world_thread_start(0xC, world_build_at_list_2);
    world_thread_set_parameters(0xC, (s32)&g_wldcore_window_panel_render_state, 0, 0);

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.list_window = index;
    {
        wldcore_point32_t* position = (wldcore_point32_t*)&g_wldcore_window_records[index].x;
        g_wldcore_window_records[index].priority = 9;
        position->x = 0xE;
        position->y = -0x29;
    }
    g_wldcore_window_records[index].sequence = 0x64;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.cursor_window = index;
    g_wldcore_window_records[index].sequence = 2;
    g_wldcore_window_records[index].priority = 9;
    g_wldcore_window_records[index].flags |= 0x10;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.layout_render = index;
    bounds.position.x = 0x80;
    bounds.position.y = 0;
    bounds.dimensions.x = 0x6C;
    bounds.dimensions.y = 0x40;
    wldcore_window_init_render_record_image(
        index, bounds.position, bounds.dimensions, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    {
        wldcore_point32_t* base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
        base->x = 0xC;
        base->y = -0x28;
    }
    DrawSync(0);

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.frame_window_a = index;
    {
        wldcore_point32_t* position = (wldcore_point32_t*)&g_wldcore_window_records[index].x;
        g_wldcore_window_records[index].priority = 9;
        position->x = -0x60;
        position->y = 0x60;
    }
    g_wldcore_window_records[index].sequence = 0x73;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.frame_window_b = index;
    {
        wldcore_point32_t* position = (wldcore_point32_t*)&g_wldcore_window_records[index].x;
        g_wldcore_window_records[index].priority = 9;
        position->x = -0x12;
        position->y = 0x60;
    }
    g_wldcore_window_records[index].sequence = 0x74;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.preview_render = index;
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= 0x10;
    {
        wldcore_point32_t* base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
        base->x = 0x11;
        base->y = 0x18;
    }

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.list_render = index;
    bounds.position.x = 0x80;
    bounds.position.y = 0x40;
    bounds.dimensions.x = 0x30;
    bounds.dimensions.y = 0x3B;
    wldcore_window_init_render_record_image(
        index, bounds.position, bounds.dimensions, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    {
        wldcore_point32_t* base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
        base->x = 0xC;
        base->y = 0x14;
    }
    DrawSync(0);

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.portrait_render = index;
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= 0x10;
    {
        wldcore_point32_t* base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
        base->x = 0x4D;
        base->y = 0x18;
    }

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.portrait_frame = index;
    bounds.position.x = 0x80;
    bounds.position.y = 0x40;
    bounds.dimensions.x = 0x30;
    bounds.dimensions.y = 0x3B;
    wldcore_window_init_render_record_image(
        index, bounds.position, bounds.dimensions, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= 0x10;
    {
        wldcore_point32_t* base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
        base->x = 0x48;
        base->y = 0x14;
    }

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.portrait_window = index;
    {
        wldcore_point32_t* position = (wldcore_point32_t*)&g_wldcore_window_records[index].x;
        g_wldcore_window_records[index].priority = 9;
        g_wldcore_window_records[index].flags |= 0x10;
        position->x = 0x3E;
        position->y = 0x28;
    }
    g_wldcore_window_records[index].sequence = 0x75;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.left_arrow = index;
    {
        wldcore_point32_t* position = (wldcore_point32_t*)&g_wldcore_window_records[index].x;
        g_wldcore_window_records[index].priority = 9;
        g_wldcore_window_records[index].flags |= 0x10;
        position->x = -0x74;
        position->y = -0x69;
    }
    g_wldcore_window_records[index].sequence = 0x66;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.right_arrow = index;
    {
        wldcore_point32_t* position = (wldcore_point32_t*)&g_wldcore_window_records[index].x;
        g_wldcore_menu_result = 0x1E0;
        g_wldcore_window_records[index].priority = 9;
        g_wldcore_window_records[index].flags |= 0x10;
        position->x = 0x60;
        position->y = -0x69;
    }
    g_wldcore_window_records[index].sequence = 0x68;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;
    world_thread_set_parameters(0xE, 0x19, 0xB812, 0);

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].send_unit.shown_preview = -1;
    g_wldcore_menu_stack_records_next[depth].send_unit.mode = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_SEND_UNIT;
    g_wldcore_menu_stack_depth = depth + 1;
}

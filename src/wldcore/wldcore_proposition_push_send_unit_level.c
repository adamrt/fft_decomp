#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/pad.h"

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
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    RECT bounds;
    s32 index;
    s32 depth;
    s32 count;

    g_wldcore_menu_scroll_offset = 0;
    g_wldcore_window_panel_render_state.selected_index = 0;
    g_wldcore_proposition_send_unit_count = 0;
    count = wldcore_proposition_load_send_unit_candidates(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit);
    layout.text_width = 0x6E;
    layout.visible_rows = count;
    origin.x = 8;
    origin.y = 0x50;
    wldcore_window_init_panel_render_state(
        0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.unit_count, &layout, &origin);
    g_wldcore_window_panel_label_state.mode[1] = MENU_TEXT_COLUMN_NUMBER_GLYPHS;
    g_wldcore_window_panel_label_state.x[1] = 0x58;
    g_wldcore_window_panel_label_state.text_ids[1] = g_wldcore_proposition_candidate_levels;
    g_wldcore_window_panel_label_state.text_colors[1] = g_wldcore_list_row_flags;
    g_wldcore_window_panel_label_state.x[0] = 0;
    g_wldcore_window_panel_render_state.header_id = 0x10;
    g_wldcore_window_panel_render_state.selected_index = 0;
    g_wldcore_menu_scroll_offset = 0;
    thread_start(WLDCORE_THREAD_SLOT_WINDOW_PANEL, world_screen_build_at_list);
    thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, (s32)&g_wldcore_window_panel_render_state, 0, 0);

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.list_frame.window_index = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        position->x = 0xE;
        position->y = -0x29;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_PROPOSITION_COUNT_LABEL;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 9;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.list_frame.render_index = index;
    bounds.x = 0x80;
    bounds.y = 0;
    bounds.w = 0x6C;
    bounds.h = 0x40;
    wldcore_window_init_render_record_image(index, bounds, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0xC;
        base->y = -0x28;
    }
    DrawSync(0);

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.frame_window_a = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        position->x = -0x60;
        position->y = 0x60;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SEND_UNIT_FRAME_A;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.frame_window_b = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        position->x = -0x12;
        position->y = 0x60;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SEND_UNIT_FRAME_B;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.preview_render = index;
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0x11;
        base->y = 0x18;
    }

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.list_render = index;
    bounds.x = 0x80;
    bounds.y = 0x40;
    bounds.w = 0x30;
    bounds.h = 0x3B;
    wldcore_window_init_render_record_image(index, bounds, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0xC;
        base->y = 0x14;
    }
    DrawSync(0);

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.portrait_render = index;
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0x4D;
        base->y = 0x18;
    }

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.portrait_frame = index;
    bounds.x = 0x80;
    bounds.y = 0x40;
    bounds.w = 0x30;
    bounds.h = 0x3B;
    wldcore_window_init_render_record_image(index, bounds, 0, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0x48;
        base->y = 0x14;
    }

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.portrait_window = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        position->x = 0x3E;
        position->y = 0x28;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SEND_UNIT_PORTRAIT;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.left_arrow = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_wldcore_window_records[index].priority = 9;
        g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        position->x = -0x74;
        position->y = -0x69;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_L1_BUTTON;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].send_unit.right_arrow = index;
    {
        point32_t* position = &g_wldcore_window_records[index].position;
        g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
        g_wldcore_window_records[index].priority = 9;
        g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
        position->x = 0x60;
        position->y = -0x69;
    }
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_R1_BUTTON;
    g_wldcore_window_records[index].frame_index = g_wldcore_window_records[index].anim_counter = 0;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CHOOSE_UNITS, 0);

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].send_unit.shown_preview = -1;
    g_wldcore_menu_stack_records_next[depth].send_unit.mode = WLDCORE_SEND_UNIT_MODE_SELECT;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_SEND_UNIT;
    g_wldcore_menu_stack_depth = depth + 1;
}

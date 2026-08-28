#include "fft/open.h"

/* Pushes the birthday confirmation controller (handler 6,
 * open_birthday_handle_confirmation_input) with its two cursor records and
 * the confirmation prompt window.
 *
 * The pushed record uses the open_birthday_confirmation_state_t layout
 * (cursor_record_36, unknown_04, menu_record_56, selected_option); it is
 * written through the generic stream_start/stream_length/field_08/field_0c
 * names because only direct array indexing keeps the target's per-field
 * absolute addressing. The prompt window's position is written through the
 * records_56 x/y view, which sets the base register before the index scale.
 * The unused array reproduces the target's 0x70-byte frame.
 */
void open_birthday_push_confirmation_controller(void) {
    RECT quad;
    s32 unused[12];
    s32 record;
    s32 controller;
    open_render_record_56_position_t* position;

    record = open_gfx_append_render_record_36(
        (open_render_record_36_t**)g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_stream_start[g_open_current_controller_index].stream_start = record;
    g_open_gfx_render_records_36[record].anim_id = 2;
    g_open_gfx_render_records_36[record].ot_layer = 3;
    g_open_gfx_render_records_36[record].frame_timer = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;

    record = open_gfx_append_render_record_36(
        (open_render_record_36_t**)g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_stream_start[g_open_current_controller_index].stream_length = record;
    g_open_gfx_render_records_36[record].anim_id = 0xE;
    g_open_gfx_render_records_36[record].ot_layer = 3;
    g_open_gfx_render_records_36[record].frame_timer = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;

    record = open_gfx_append_render_record_56(
        (open_render_record_56_t**)g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_stream_start[g_open_current_controller_index].field_08 = record;
    quad.x = 0;
    quad.y = 0x60;
    quad.w = 0x28;
    quad.h = 0x30;
    open_text_init_menu_render_record(record, quad, 0, 0xB80F, g_open_birthday_title_image);
    controller = g_open_current_controller_index;
    position = g_open_gfx_record_translation;
    position += record;
    position->x = 0x3C;
    position->y = 0x28;
    g_open_gfx_render_records_56[record].ot_layer = 3;

    g_open_gfx_render_records_36[g_open_controller_stream_start[controller].stream_start].x
        = g_open_gfx_render_records_56[record].x + 4;
    g_open_gfx_render_records_36[g_open_controller_stream_start[controller].stream_start].y
        = g_open_gfx_render_records_56[record].y + 0xC;
    g_open_gfx_render_records_36[g_open_controller_stream_start[controller].stream_length].x
        = g_open_gfx_render_records_56[record].x + 2;
    g_open_current_controller_index = controller + 1;
    g_open_gfx_render_records_36[g_open_controller_stream_start[controller].stream_length].y
        = g_open_gfx_render_records_56[record].y - 2;
    g_open_controller_stream_start[controller].field_0c = 0;
    g_open_controller_handler_indices[controller] = 6;
}

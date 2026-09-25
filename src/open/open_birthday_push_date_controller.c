#include "fft/open.h"

/* Pushes the birthday date-entry controller (handler 5,
 * open_birthday_handle_menu_input) with its cursor record and the date prompt
 * record, then seeds month/day to 1 and builds the menu text.
 *
 * Shaped like open_birthday_push_confirmation_controller: the controller record
 * is reached through g_open_controller_stream_start[g_open_current_controller_index]
 * on every use (the target reloads the index after each call rather than
 * keeping it in a local), and the prompt record's position is written through
 * the records_56 x/y view so the base register is set before the index scale.
 *
 * One `record` variable serves both the 36- and the 56-byte allocation; two
 * variables let the first live in v0 and drop s2 from the frame. `s32
 * unused[4]` is the 0x10-byte frame hole the declared locals cannot explain,
 * giving the target's 0x48-byte frame. `half_width` is its own statement
 * before the position stores, which hoists the `lhu` of quad.w into the
 * load-delay slot instead of emitting a nop.
 */
void open_birthday_push_date_controller(void) {
    RECT quad;
    s32 unused[4];
    s32 record;
    s32 controller;
    s32 half_width;
    open_render_record_56_position_t* position;

    record
        = open_gfx_append_render_record_36(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_stream_start[g_open_current_controller_index].stream_start = record;
    g_open_gfx_render_records_36[record].anim_id = 0xC;
    g_open_gfx_render_records_36[record].ot_layer = 4;
    g_open_gfx_render_records_36[record].frame_timer = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;

    record
        = open_gfx_append_render_record_56(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_stream_start[g_open_current_controller_index].field_08 = record;
    quad.x = 0;
    quad.y = 0;
    quad.w = 0x80;
    quad.h = 0x40;
    open_text_init_menu_render_record(record, quad, 0, 0xB800, g_open_birthday_title_image);
    position = g_open_gfx_record_translation;
    position += record;
    half_width = quad.w / 2;
    position->y = -0x60;
    position->x = -half_width;
    g_open_gfx_render_records_56[record].ot_layer = 4;
    g_open_controller_stream_start[g_open_current_controller_index].field_20 = 1;
    g_open_controller_stream_start[g_open_current_controller_index].field_24 = 1;
    open_birthday_build_menu_text(
        (open_birthday_date_state_t*)&g_open_controller_stream_start[g_open_current_controller_index]);

    controller = g_open_current_controller_index;
    g_open_gfx_render_records_56[g_open_controller_stream_start[controller].field_0c].flags |= 0x100;
    g_open_gfx_render_records_36[g_open_controller_stream_start[controller].stream_start].x
        = g_open_gfx_render_records_56[g_open_controller_stream_start[controller].field_0c].x + 0x46;
    g_open_current_controller_index = controller + 1;
    g_open_gfx_render_records_36[g_open_controller_stream_start[controller].stream_start].y
        = g_open_gfx_render_records_56[g_open_controller_stream_start[controller].field_0c].y + 0x18;
    g_open_controller_stream_start[controller].field_28 = 0;
    g_open_controller_handler_indices[controller] = 5;
}

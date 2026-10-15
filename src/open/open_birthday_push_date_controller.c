#include "fft/open.h"

/* Pushes the birthday date-entry controller (handler 5,
 * open_birthday_handle_menu_input) with its cursor record and the date prompt
 * record, then seeds month/day to 1 and builds the menu text.
 *
 * Shaped like open_birthday_push_confirmation_controller: the controller record
 * is reached through g_open_controller_records_next[g_open_current_controller_index]
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

    record = window_append_animated_record_variant(
        g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_records_next[g_open_current_controller_index].birthday_date.cursor_record_36 = record;
    g_open_gfx_render_records_36[record].sequence = 0xC;
    g_open_gfx_render_records_36[record].priority = 4;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;

    record = window_append_record_variant(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_records_next[g_open_current_controller_index].birthday_date.title_record_56 = record;
    quad.x = 0;
    quad.y = 0;
    quad.w = 0x80;
    quad.h = 0x40;
    window_init_text_record_variant(record, quad, 0, 0xB800, g_open_birthday_title_image);
    position = g_open_gfx_record_translation;
    position += record;
    half_width = quad.w / 2;
    position->y = -0x60;
    position->x = -half_width;
    g_open_gfx_render_records_56[record].ot_layer = 4;
    g_open_controller_records_next[g_open_current_controller_index].birthday_date.month = 1;
    g_open_controller_records_next[g_open_current_controller_index].birthday_date.day = 1;
    open_birthday_build_menu_text(&g_open_controller_records_next[g_open_current_controller_index].birthday_date);

    controller = g_open_current_controller_index;
    g_open_gfx_render_records_56[g_open_controller_records_next[controller].birthday_date.window_record_56].flags
        |= WORLD_RENDER_RECORD_POP_IN;
    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_date.cursor_record_36].position.x
        = g_open_gfx_render_records_56[g_open_controller_records_next[controller].birthday_date.window_record_56].x
        + 0x46;
    g_open_current_controller_index = controller + 1;
    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_date.cursor_record_36].position.y
        = g_open_gfx_render_records_56[g_open_controller_records_next[controller].birthday_date.window_record_56].y
        + 0x18;
    g_open_controller_records_next[controller].birthday_date.selecting_month = 0;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_BIRTHDAY_DATE;
}

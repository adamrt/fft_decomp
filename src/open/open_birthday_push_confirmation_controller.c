#include "fft/open.h"

/* Pushes the birthday confirmation controller (handler 6,
 * open_birthday_handle_confirmation_input) with its two cursor records and
 * the confirmation prompt window.
 *
 * The pushed record is written member by member through the record array:
 * only direct array indexing keeps the target's per-field absolute
 * addressing. The prompt window's position is written through the
 * records_56 x/y view, which sets the base register before the index scale.
 * The unused array reproduces the target's 0x70-byte frame.
 */
void open_birthday_push_confirmation_controller(void) {
    RECT quad;
    s32 unused[12];
    s32 record;
    s32 controller;
    open_render_record_56_position_t* position;

    record = window_append_animated_record_variant(
        g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_records_next[g_open_current_controller_index].birthday_confirmation.cursor_record_36 = record;
    g_open_gfx_render_records_36[record].sequence = 2;
    g_open_gfx_render_records_36[record].priority = 3;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;

    record = window_append_animated_record_variant(
        g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_records_next[g_open_current_controller_index].birthday_confirmation.label_record = record;
    g_open_gfx_render_records_36[record].sequence = 0xE;
    g_open_gfx_render_records_36[record].priority = 3;
    g_open_gfx_render_records_36[record].anim_counter = 0;
    g_open_gfx_render_records_36[record].frame_index = 0;

    record = window_append_record_variant(g_open_gfx_render_record_pointers, &g_open_gfx_render_record_pointer_count);
    g_open_controller_records_next[g_open_current_controller_index].birthday_confirmation.menu_record_56 = record;
    quad.x = 0;
    quad.y = 0x60;
    quad.w = 0x28;
    quad.h = 0x30;
    window_init_text_record_variant(record, quad, 0, 0xB80F, g_open_birthday_title_image);
    controller = g_open_current_controller_index;
    position = g_open_gfx_record_translation;
    position += record;
    position->x = 0x3C;
    position->y = 0x28;
    g_open_gfx_render_records_56[record].ot_layer = 3;

    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_confirmation.cursor_record_36]
        .position.x = g_open_gfx_render_records_56[record].x + 4;
    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_confirmation.cursor_record_36]
        .position.y = g_open_gfx_render_records_56[record].y + 0xC;
    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_confirmation.label_record]
        .position.x = g_open_gfx_render_records_56[record].x + 2;
    g_open_current_controller_index = controller + 1;
    g_open_gfx_render_records_36[g_open_controller_records_next[controller].birthday_confirmation.label_record]
        .position.y = g_open_gfx_render_records_56[record].y - 2;
    g_open_controller_records_next[controller].birthday_confirmation.selected_option = 0;
    g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_BIRTHDAY_CONFIRMATION;
}

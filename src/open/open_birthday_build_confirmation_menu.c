#include "fft/open.h"

/* Recolor the birthday menu's render records for the confirmation prompt:
 * dim the date cursor, gray the label, and mark both windows as step 2.
 *
 * The target reserves a 24-byte frame it never touches (the stack adjustment
 * is even scheduled into the last load-delay slot), so the original body
 * declared an unused aggregate local of 17..24 bytes. */
void open_birthday_build_confirmation_menu(open_birthday_date_state_t* state) {
    s32 unused_scratch[6];

    g_open_gfx_render_records_36[state->cursor_record_36].sequence = 10;
    g_open_gfx_render_records_36[state->cursor_record_36].frame_index
        = g_open_gfx_render_records_36[state->cursor_record_36].anim_counter = 0;

    g_open_gfx_render_records_36[state->label_record].color.r = 0x40;
    g_open_gfx_render_records_36[state->label_record].color.g = 0x40;
    g_open_gfx_render_records_36[state->label_record].color.b = 0x40;

    g_open_gfx_render_records_36[state->cursor_record_36].palette = 7;

    g_open_gfx_render_records_56[state->title_record_56].palette = 2;
    g_open_gfx_render_records_56[state->window_record_56].palette = 2;
}

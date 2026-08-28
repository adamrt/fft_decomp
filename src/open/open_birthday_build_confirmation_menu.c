#include "fft/open.h"

/* Recolour the birthday menu's render records for the confirmation prompt:
 * dim the date cursor, grey the label, and mark both windows as step 2.
 *
 * The controller record is re-read before every store, so it is addressed
 * through a volatile alias.
 *
 * The target reserves a 24-byte frame it never touches (the stack adjustment
 * is even scheduled into the last load-delay slot), so the original body
 * declared an unused aggregate local of 17..24 bytes. */
void open_birthday_build_confirmation_menu(const open_birthday_date_state_t* menu) {
    volatile const open_birthday_date_state_t* state = menu;
    s32 unused_scratch[6];
    s32 cursor;

    g_open_gfx_render_records_36[state->cursor_record_36].anim_id = 10;

    cursor = state->cursor_record_36;
    g_open_gfx_render_records_36[cursor].frame_timer = 0;
    g_open_gfx_render_records_36[cursor].frame_index = 0;

    g_open_gfx_render_records_36[state->unknown_04].r = 0x40;
    g_open_gfx_render_records_36[state->unknown_04].g = 0x40;
    g_open_gfx_render_records_36[state->unknown_04].b = 0x40;

    g_open_gfx_render_records_36[state->cursor_record_36].palette = 7;

    g_open_gfx_render_records_56[state->title_record_56].palette = 2;
    g_open_gfx_render_records_56[state->window_record_56].palette = 2;
}

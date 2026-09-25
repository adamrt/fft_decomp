#include "fft/open.h"

/* Restore the date-selection records after leaving the confirmation prompt. */
void open_restore_birthday_date_menu(open_birthday_date_state_t* menu) {
    open_render_record_56_t* records_56;
    /* The target reserves a 24-byte frame it never touches, with the stack
     * adjustment scheduled into a load-delay slot, exactly as in the twin
     * open_birthday_build_confirmation_menu. */
    s32 unused_scratch[6];
    s32 cursor;
    s32 cursor_last;
    s32 other_record_56;

    g_open_gfx_render_records_36[menu->cursor_record_36].anim_id = 12;

    cursor = menu->cursor_record_36;
    g_open_gfx_render_records_36[cursor].frame_timer = 0;
    g_open_gfx_render_records_36[cursor].frame_index = 0;

    g_open_gfx_render_records_36[menu->label_record].color.r = 0x80;
    g_open_gfx_render_records_36[menu->label_record].color.g = 0x80;
    g_open_gfx_render_records_36[menu->label_record].color.b = 0x80;

    /* Keeps the base in its own register ($a2) at this position; CSE otherwise folds each store through $at. */
    records_56 = g_open_gfx_render_records_56;
    __asm__ volatile("" : "=r"(records_56) : "0"(records_56));

    cursor_last = menu->cursor_record_36;
    /* The target issues this load at +0xe0, interleaved into the preceding
     * store's address computation. GCC 2.6.3 will not hoist it itself: both
     * the store and the load are struct members, so MEM_IN_STRUCT_P refuses
     * to move the load across the store, and the load-delay slot costs a nop. */
    other_record_56 = menu->title_record_56;
    records_56[menu->window_record_56].palette = 0;
    records_56[other_record_56].palette = 0;

    g_open_gfx_render_records_36[cursor_last].palette = 0;
}
